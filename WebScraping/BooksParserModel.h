#pragma once
#include <iostream>
#include "BookListing.h"
#include <tbb/task_group.h>
#include <tbb/tick_count.h>
#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_hash_map.h>
#include <curl/curl.h>
class BooksParserModel {


private:
	struct PageData {
		std::string url;
		std::string html;
	};
	// storing fetched and parsed books
	tbb::concurrent_vector<BookListing> parsedBooks;

	// key: number of stars; value: number of books with that number of stars
	tbb::concurrent_hash_map<int, int> starsAnalytics;

	// storing titles where searched word was found
	tbb::concurrent_vector<std::string> matchedTitles;

	// the cheapest and the most expensive books for analitycs
	BookListing cheapestBook;
	BookListing mostExpensiveBook;

	// storing sum of prices of books, for calculating average price
	std::atomic<double> totalPrice{ 0.0 };

	// root url, cannot be changed
	const std::string rootUrl = "https://books.toscrape.com/catalogue/";

	// filename for writing
	const std::string filename = "lastResult.txt";

	// indexes found
	std::vector<int> indexes;
	bool printFetchingProgress;
	std::string searchedText;

	int extractInt(const string& input);
	BookListing extractBookListing(const std::string& html);
	std::vector<BookListing> extractBookListings(const std::string& html);
	bool findSubstring(std::string string, std::string substring);
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
	std::string perform_request(const std::string& url);
	std::vector<PageData> indexSite();
	std::vector<BookListing> parallelFetchAndParse();
	std::vector<BookListing> serialFetchAndParse();
	std::string getResultsString(const double& timeNeeded, const std::string& typeOfFetch);
	void printStart();
	void printResult();
	void writeResultsToFile(std::string content);
public:
	BooksParserModel(bool printProgress = false);
	~BooksParserModel();
	void start(const std::string& typeOfFetch);
};