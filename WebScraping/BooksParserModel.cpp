#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _HAS_STD_BYTE 0

#include <iostream>
#include <vector>
#include "BookListing.h"
#include <tbb/task_group.h>
#include <tbb/tick_count.h>
#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/mutex.h>
#include <curl/curl.h>
#include <tbb/tick_count.h>
#include "BooksParserModel.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>

BooksParserModel::BooksParserModel(bool printProgress) : printFetchingProgress(printProgress) {
    for (int i = 1; i <= 5; i++) {
        tbb::concurrent_hash_map<int, int>::accessor acc;
        starsAnalytics.insert(acc, i);
        acc->second = 0;
    }
}

int BooksParserModel::extractInt(const std::string& input) {
    if (input == "One") {
        return 1;
    }
    else if (input == "Two") {
        return 2;
    }
    else if (input == "Three") {
        return 3;
    }
    else if (input == "Four") {
        return 4;
    }
    else if (input == "Five") {
        return 5;
    }
    else {
        return 0;
    }

}

BookListing BooksParserModel::extractBookListing(const std::string& html) {
    std::string title;
    double price = 0.0;
    int starsNum = 0;

    const std::string titleKey = "title=\"";
    size_t titlePos = html.find(titleKey);
    if (titlePos != std::string::npos) {
        size_t start = titlePos + titleKey.size();
        size_t end = html.find('"', start);
        if (end != std::string::npos) {
            title = html.substr(start, end - start);
        }
    }

    const std::string starKey = "star-rating ";
    size_t starPos = html.find(starKey);
    if (starPos != std::string::npos) {
        size_t start = starPos + starKey.size();
        size_t end = html.find('"', start);
        std::string starsWord = html.substr(start, end - start);
        starsNum = extractInt(starsWord);
    }

    const std::string priceKey = "price_color\">";
    size_t pricePos = html.find(priceKey);
    if (pricePos != std::string::npos) {
        size_t start = pricePos + priceKey.size();
        size_t end = html.find('<', start);
        if (end != std::string::npos) {
            std::string priceStr = html.substr(start + 2, end - start - 2);
            price = std::stod(priceStr);
        }
    }

    return BookListing(title, price, starsNum);
}

std::vector<BookListing> BooksParserModel::extractBookListings(const std::string& html) {
    std::vector<BookListing> listings;
    const std::string articleStart = "<article";
    const std::string articleEnd = "</article>";

    size_t pos = 0;
    while (pos < html.size()) {
        size_t articleStartPos = html.find(articleStart, pos);
        if (articleStartPos == std::string::npos) break;

        size_t articleEndPos = html.find(articleEnd, articleStartPos);
        if (articleEndPos == std::string::npos) break;

        size_t contentStart = articleStartPos;
        size_t contentLength = articleEndPos + articleEnd.size() - contentStart;
        std::string articleHtml = html.substr(contentStart, contentLength);

        BookListing bl = extractBookListing(articleHtml);
        listings.push_back(bl);

        pos = articleEndPos + articleEnd.size();
    }

    return listings;
}


bool BooksParserModel::findSubstring(std::string string, std::string substring) {
    std::transform(string.begin(), string.end(), string.begin(),
        [](unsigned char c) { return std::tolower(c); });

    std::transform(substring.begin(), substring.end(), substring.begin(),
        [](unsigned char c) { return std::tolower(c); });


    if (string.find(substring) != std::string::npos) {
        return true;
    }
    else {
        return false;
    }
}

size_t BooksParserModel::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string BooksParserModel::perform_request(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &BooksParserModel::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed for " << url
                << ": " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return "";
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (http_code == 404) {
            std::cerr << "Got 404 for " << url << std::endl;
            return "";
        }

        if (this->printFetchingProgress) {
            std::cout << "Successfully fetched " << url
                << " (HTTP " << http_code << "). Data size: "
                << readBuffer.length() << std::endl;
        }

        return readBuffer;
    }
    else {
        std::cerr << "Failed to initialize CURL handle for " << url << std::endl;
        return "";
    }
}

std::vector<BooksParserModel::PageData> BooksParserModel::indexSite() {
    tbb::concurrent_vector<PageData> pages;
    std::atomic<bool> stopFetching{ false };
    tbb::task_group tg;
    for (int i = 1; i > 0; ++i) {
        if (stopFetching.load()) break;
        tg.run([i, &pages, &stopFetching, this] {
            if (stopFetching.load()) return;
            std::string url = this->rootUrl + "page-" + std::to_string(i) + ".html";
            std::cout << "Fetching: " << url << std::endl;
            std::string html = perform_request(url);
            if (html.empty()) {
                stopFetching.store(true);
                return;
            }
            pages.push_back({ url, std::move(html) });
            });
    }
    tg.wait();
    return std::vector<PageData>(pages.begin(), pages.end());
}

std::vector<BookListing> BooksParserModel::parallelFetchAndParse() {
    tbb::task_group tg;
    tbb::mutex minMaxMutex;

    std::vector<BooksParserModel::PageData> pages = this->indexSite();
    for (const BooksParserModel::PageData& page : pages) {
        tg.run([this, page, &minMaxMutex] {
            std::vector<BookListing> books = extractBookListings(page.html);

            int localFiveStarCount = 0;
            double localTotalPrice = 0.0;

            for (BookListing& b : books) {
                parsedBooks.push_back(b);

                tbb::concurrent_hash_map<int, int>::accessor acc;
                if (starsAnalytics.find(acc, b.getStarsNum())) {
                    acc->second++;
                }

                localTotalPrice += b.getPrice();

                if (this->searchedText != "") {
                    if (findSubstring(b.getTitle(), this->searchedText)) {
                        matchedTitles.push_back(b.getTitle());
                    }
                }

                tbb::mutex::scoped_lock lock(minMaxMutex);
                if (cheapestBook.getTitle().empty() || b.getPrice() < cheapestBook.getPrice()) {
                    cheapestBook = b;
                }
                if (mostExpensiveBook.getTitle().empty() || b.getPrice() > mostExpensiveBook.getPrice()) {
                    mostExpensiveBook = b;
                }

            }
            totalPrice.fetch_add(localTotalPrice);
        });
    }

    tg.wait();
    
    return std::vector<BookListing>(parsedBooks.begin(), parsedBooks.end());
}

std::vector<BookListing> BooksParserModel::serialFetchAndParse() {
    for (int i = 1; i <= 50; ++i) {
        std::string fetchingUrl = this->rootUrl + "page-" + to_string(i) + ".html";
        std::cout << "Fetching: " << fetchingUrl << std::endl;
        std::string html = perform_request(fetchingUrl);
        if (!html.empty()) {
            std::vector<BookListing> books = extractBookListings(html);

            int localFiveStarCount = 0;
            double localTotalPrice = 0.0;

            for (BookListing& b : books) {
                parsedBooks.push_back(b);

                tbb::concurrent_hash_map<int, int>::accessor acc;
                if (starsAnalytics.find(acc, b.getStarsNum())) {
                    acc->second++;
                }

                localTotalPrice += b.getPrice();

                if (this->searchedText != "") {
                    if (findSubstring(b.getTitle(), this->searchedText)) {
                        matchedTitles.push_back(b.getTitle());
                    }
                }

                if (cheapestBook.getTitle().empty() || b.getPrice() < cheapestBook.getPrice()) {
                    cheapestBook = b;
                }
                if (mostExpensiveBook.getTitle().empty() || b.getPrice() > mostExpensiveBook.getPrice()) {
                    mostExpensiveBook = b;
                }

            }
            this->totalPrice += localTotalPrice;
        }
    }


    return std::vector<BookListing>(parsedBooks.begin(), parsedBooks.end());
}

BooksParserModel::~BooksParserModel() {

}

void BooksParserModel::printResult() {
    int totalBooks = this->parsedBooks.size();
    double averagePrice = totalBooks > 0 ? this->totalPrice.load() / totalBooks : 0.0;
    std::cout << "\nStar Rating Analytics:" << std::endl;
    for (int i = 1; i <= 5; i++) {
        tbb::concurrent_hash_map<int, int>::const_accessor ca;
        if (this->starsAnalytics.find(ca, i)) {
            std::cout << "Number of books with " << i << " star(s): " << ca->second << std::endl;
        }
    }
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "Average price: \xC2\xA3" << averagePrice << std::endl;
    if (this->searchedText != "") {
        std::cout << "Matched books with search \"" << this->searchedText <<"\": " << std::endl;
        for (std::string title : matchedTitles) {
            std::cout << title << std::endl;
        }
    }
    else {
        std::cout << "The search was not used." << std::endl;
    }
}

void BooksParserModel::printStart() {
    std::cout << "Welcome to the scraper for the books.toscrape.com website!" << std::endl;
    std::string line;
    std::cout << "What would you like to search: ";
    std::getline(std::cin, line);
    this->searchedText = line;
}

std::string BooksParserModel::getResultsString(const double& timeNeeded, const std::string& typeOfFetch) {
    std::stringstream ss;
    if (typeOfFetch == "parallel") {
        ss << "Parallel fetching results" << std::endl;
    }
    else {
        ss << "Serial fetching results" << std::endl;
    }
    ss << "====================================================" << std::endl;
    ss << "Calcualtions took " << timeNeeded << " seconds." << endl;
    ss << "Time per book: " << timeNeeded / this->parsedBooks.size() << " seconds." << std::endl;
    ss << "Time per page: " << timeNeeded / 50 << " seconds." << std::endl;
    ss << "====================================================" << std::endl;

    int totalBooks = this->parsedBooks.size();
    double averagePrice = totalBooks > 0 ? this->totalPrice.load() / totalBooks : 0.0;
    ss << "Star Rating Analytics:" << std::endl;
    int starsNumSum = 0;
    for (int i = 1; i <= 5; i++) {
        tbb::concurrent_hash_map<int, int>::const_accessor ca;
        if (this->starsAnalytics.find(ca, i)) {
            ss << "Number of books with " << i << " star(s): " << ca->second << std::endl;
            starsNumSum += i * ca->second;
        }
    }
    ss << "Average rating: " << static_cast<double>(starsNumSum) / 1000 << " stars." << std::endl;
    ss << "====================================================" << std::endl;
    ss << "Average price of book: " << averagePrice << " GBP." << std::endl;
    ss << "----------------------------------------------------" << std::endl;
    ss << "The cheapest book:" << std::endl;
    ss << this->cheapestBook << std::endl;
    ss << "----------------------------------------------------" << std::endl;
    ss << "The most expensive book:" << std::endl;
    ss << this->mostExpensiveBook << std::endl;
    ss << "====================================================" << std::endl;
    if (this->searchedText != "") {
        ss << "Matched books with search \"" << this->searchedText << "\": " << std::endl;
        if (this->matchedTitles.size() == 0) {
            ss << "No matching books were found!" << std::endl;
        }
        else {
            for (const std::string& title : this->matchedTitles) {
                ss << title << std::endl;
            }
        }
    }
    else {
        ss << "The search was not used." << std::endl;
    }
    ss << "====================================================" << std::endl;

    return ss.str();
}

void BooksParserModel::writeResultsToFile(std::string content) {
    if (std::filesystem::exists(this->filename)) {
        std::cout << "File exists, overwriting..." << std::endl;
    }
    else {
        std::cout << "Creating new file..." << std::endl;
    }

    std::ofstream file(this->filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    file << content;
    file.close();
    std::cerr << "Result writing in file successful!" << std::endl;
}

void BooksParserModel::start(const std::string& typeOfFetch) {
    if (typeOfFetch != "serial" && typeOfFetch != "parallel") {
        cout << "You need to input \"serial\" or \"parallel\" as an arguemnt!" << endl;
        return;
    }
    this->printStart();
    if (typeOfFetch == "serial") {
        cout << "Doing serial fetch and parse..." << endl;
    }
    else if (typeOfFetch == "parallel") {
        cout << "Doing parallel fetch and parse..." << endl;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    tbb::tick_count startTime = tbb::tick_count::now();
    if (typeOfFetch == "parallel") {
        parallelFetchAndParse();
    }
    else {
        serialFetchAndParse();
    }
    tbb::tick_count endTime = tbb::tick_count::now();
    curl_global_cleanup();
    cout << "Done!" << endl;
    double timeNeeded = (endTime - startTime).seconds();
    std::string resultContent = this->getResultsString(timeNeeded, typeOfFetch);
    cout << resultContent;
    this->writeResultsToFile(resultContent);
}