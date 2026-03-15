#pragma once
#include <iostream>
#include <string>
using namespace std;

class BookListing {
private:
	string title;
	double price;
	int starsNum;

public:
	BookListing();

	BookListing(string _title, double _price, int _starsNum);

	string getTitle();

	double getPrice();

	int getStarsNum();

	friend ostream& operator<<(ostream& os, BookListing bl);
}
;