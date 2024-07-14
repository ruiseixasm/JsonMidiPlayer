#include <iostream>
#include <list>
#include <algorithm>

// Define the Item class
class Item {
public:
    std::string name;
    double price;

    // Constructor
    Item(std::string n, double p) : name(n), price(p) {}

    // Friend function to print the Item
    friend std::ostream& operator<<(std::ostream& os, const Item& item);
};

// Overload the << operator to print Item objects
std::ostream& operator<<(std::ostream& os, const Item& item) {
    os << "Item(name: " << item.name << ", price: " << item.price << ")";
    return os;
}

// Comparator function to sort items by price
bool compareByPrice(const Item& a, const Item& b) {
    return a.price < b.price;
}

int main() {
    // Create a list of Item objects
    std::list<Item> itemList = {
        Item("Apple", 1.99),
        Item("Banana", 0.99),
        Item("Orange", 2.49),
        Item("Mango", 1.49)
    };

    // Sort the list by price using the compareByPrice function
    itemList.sort(compareByPrice);

    // Print the sorted list
    for (const auto& item : itemList) {
        std::cout << item << std::endl;
    }

    return 0;
}
