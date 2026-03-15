// WebScraping.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "httplib.h"
#include "BooksParserModel.h"


int main()
{
    BooksParserModel model(false);
    model.start("parallel");
}
