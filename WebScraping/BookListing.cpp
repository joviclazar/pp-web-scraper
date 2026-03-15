#include <iostream>
#include <string>
using namespace std;
#include "BookListing.h"
#include <iomanip>

BookListing::BookListing() : title(""), price(0.0), starsNum(0) {}

BookListing::BookListing(string _title, double _price, int _starsNum) :
	title(_title), price(_price), starsNum(_starsNum) {}

string BookListing::getTitle() {
	return title;
}

double BookListing::getPrice() {
	return price;
}

int BookListing::getStarsNum() {
	return starsNum;
}

ostream& operator<<(ostream& os, BookListing bl) {
	os << "Title: " << bl.title << "\nPrice: " << std::fixed << std::setprecision(2)
		<< bl.price << "\nStars: " << bl.starsNum;
	return os;
}