#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\python\\CConfigurationManager.h"
/**
 * @file ConfigurationManager.h
 * @brief Configuration manager for the fish lamp system
 * 
 * Manages configuration settings including RGB LED schedules,
 * sunrise/sunset offsets, and other light control parameters.
 */

#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

// Structure to represent a single color in RGB format
typedef struct {
    uint8_t red;      // Red component (0-255)
    uint8_t green;    // Green component (0-255)
    uint8_t blue;     // Blue component (0-255)
    uint8_t alpha;    // Alpha/transparency (0-255)
} rgbColor_t;

// Structure to represent a time period with RGB values
typedef struct {
    uint32_t startDelay;      // Start delay in milliseconds
    uint32_t duration;         // Duration in milliseconds
    rgbColor_t color;          // RGB color value
} timePeriod_t;

// Structure to represent the full light schedule
typedef struct {
    timePeriod_t* periods;     // Array of time periods
    uint16_t count;            // Number of periods
    bool hasGrowLightConfig;   // Flag for growlight configuration
    double maxOffset;          // Maximum offset for growlight (in milliseconds)
} schedule_t;

// Structure to hold all configuration settings
typedef struct {
    uint64_t createDate;       // Creation date timestamp
    uint64_t modDate;          // Last modification date timestamp
    
    // Light schedule
    schedule_t lightSchedule;  // RGB light schedule
    
    // Sunrise/sunset offsets (in milliseconds)
    double sunriseOffset;      // Offset after sunset
    double growlightMaxOffset; // Maximum offset before sunrise for growlight control
    
} configuration_t;

class ConfigurationManager {
public:
    /**
     * @brief Default constructor
     */
    ConfigurationManager();
    
    /**
     * @brief Copy constructor
     */
    ConfigurationManager(const ConfigurationManager& other);
    
    /**
     * @brief Destructor
     */
    ~ConfigurationManager();
    
    /**
     * @brief Load configuration from file (config.json)
     * @return true if successful, false otherwise
     */
    bool loadFromFile(const std::string& filename = "config.json");
    
    /**
     * @brief Save configuration to file
     * @return true if successful, false otherwise
     */
    bool saveToFile(const std::string& filename = "config.json");
    
    /**
     * @brief Load configuration from stdin (for testing)
     * @return true if successful, false otherwise
     */
    bool loadFromStdin();
    
    /**
     * @brief Save configuration to stdout (for testing)
     * @return true if successful, false otherwise
     */
    bool saveToStdout();
    
    /**
     * @brief Set the creation date timestamp
     * @param timestamp Unix timestamp in milliseconds
     */
    void setCreateDate(uint64_t timestamp);
    
    /**
     * @brief Set the modification date timestamp
     * @param timestamp Unix timestamp in milliseconds
     */
    void setModDate(uint64_t timestamp);
    
    /**
     * @brief Get the current modification date
     * @return Current modification timestamp or 0 if not set
     */
    uint64_t getModDate() const;
    
    /**
     * @brief Update the modification date
     */
    void updateModificationDate();
    
    // Accessors for configuration values
    /**
     * @brief Get sunrise offset (milliseconds)
     */
    double getSunriseOffset() const;
    
    /**
     * @brief Set sunrise offset (milliseconds)
     * @param offset Offset value in milliseconds
     */
    void setSunriseOffset(double offset);
    
    /**
     * @brief Get growlight max offset (milliseconds)
     */
    double getGrowlightMaxOffset() const;
    
    /**
     * @brief Set growlight max offset (milliseconds)
     * @param offset Offset value in milliseconds
     */
    void setGrowlightMaxOffset(double offset);
    
    /**
     * @brief Check if configuration has grow light data
     */
    bool hasGrowLightConfig() const;
    
    // Accessors for light schedule
    /**
     * @brief Get total number of time periods
     */
    uint16_t getPeriodCount() const;
    
    /**
     * @brief Get start delay in milliseconds
     */
    uint32_t getStartDelay() const;
    
    /**
     * @brief Set start delay in milliseconds
     * @param delay Start delay value
     */
    void setStartDelay(uint32_t delay);
    
    /**
     * @brief Get the color for a specific period index (0-based)
     * @param index Period index
     * @return RGB color structure
     */
    rgbColor_t getPeriodColor(uint16_t index) const;
    
    /**
     * @brief Set the color for a specific period index
     * @param index Period index
     * @param color RGB color value
     */
    void setPeriodColor(uint16_t index, const rgbColor_t& color);
    
    // Light schedule manipulation methods
    /**
     * @brief Get all periods as vector
     */
    std::vector<timePeriod_t> getPeriodsAsVector() const;
    
    /**
     * @brief Add a new period to the end of the schedule
     * @param startDelay Start delay in milliseconds
     * @param duration Duration in milliseconds
     * @param color RGB color value
     * @return true if successful, false otherwise
     */
    bool addPeriod(uint32_t startDelay, uint32_t duration, const rgbColor_t& color);
    
    /**
     * @brief Remove the last period from the schedule
     * @return true if successful, false otherwise
     */
    bool removeLastPeriod();
    
    /**
     * @brief Get total duration of all periods in milliseconds
     */
    uint32_t getTotalDuration() const;

private:
    configuration_t config_;  // Configuration structure
    
    /**
     * @brief Helper method to create an RGB color from JSON array [R, G, B, A]
     */
    rgbColor_t createRGBFromJSON(const std::vector<int>& jsonArray);
    
    /**
     * @brief Helper method to create a time period structure
     */
    timePeriod_t createTimePeriod(uint32_t delay, uint32_t duration, 
                                   const rgbColor_t& color);
    
    /**
     * @brief Helper method to serialize configuration to JSON string
     */
    std::string serializeToJson() const;
    
    /**
     * @brief Helper method to deserialize configuration from JSON string
     */
    bool deserializeFromJson(const std::string& json);
    
    /**
     * @brief Helper method to read a period's data line from JSON
     */
    uint32_t readPeriodLine(std::istream& input, 
                             uint32_t& delay, uint32_t& duration);
};

#endif // CONFIGURATION_MANAGER_H
