#include "temp_logic.h"
#include <string>

using std::string;

string isOverThreshold(float temp, float threshold) {
    string result = "Nhiệt độ hiện tại: " + std::to_string(temp) +
                    "°C, ngưỡng: " + std::to_string(threshold) + "°C. ";
    if (temp > threshold) {
        result += "Vượt ngưỡng!";
    } else {
        result += "Không vượt ngưỡng.";
    }
    return result;
}