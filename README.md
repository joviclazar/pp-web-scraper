# Parallel Web Scraper in C++ 
This project is a parallel web scraper implemented in C++ using the Intel Threading Building Blocks (TBB) library. It is designed to scrape data from [books.toscrape.com](http://books.toscrape.com/) using multiple threads to improve performance. This project was part of Parallel Programming course at the Faculty of Tehnical Sciences, University of Novi Sad.

## Features
- Parallel web scraping using C++ threads with tbb library
- Scrapes book information (title, price, rating) from [books.toscrape.com](http://books.toscrape.com/)
- Calculates average price of the books
- Identifies the cheapest and most expensive books
- Counts the number of books for each rating
- Search functionality to find books based on user input
All of the above features are working in a parallel manner to enhance performance and efficiency, ensuring there is no data race.

- Results displayed in a user-friendly format
- Saves results to `lastResult.txt` for future reference

## How to compile and run
1. Make sure you have a C++ compiler installed (e.g., g++).
2. Clone the repository or download the source code.
3. Open a terminal and navigate to the project directory.
4. Compile the code using the following command:
    ```
    g++ -o web_scraper main.cpp
    ```
5. Run the scraper with the command:
    ```
   ./web_scraper
    ```
6. Follow the on-screen prompts to view results and perform searches.

Additionally, in the `WebScrapping.cpp` file, you can change the mode of the program in the `main` function in:
    ```
    BooksParserModel model(false);
    model.start("parallel");
    ```
To `model.start` function you can pass either "parallel" or "serial" to run the scraper in parallel or sequential mode respectively.

## Dependencies
- C++11 or later
- tbb library for parallelism (if using parallel mode)

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
