#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <map>
#include <chrono>
#include <random>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <array>
#include <format>

#include "zdbpp.h"

using namespace zdb;

const std::map<std::string_view, std::string_view> data {
    {"Fry",                 "Ceci n'est pas une pipe"},
    {"Leela",               "Mona Lisa"},
    {"Bender",              "Bryllup i Hardanger"},
    {"Farnsworth",          "The Scream"},
    {"Zoidberg",            "Vampyre"},
    {"Amy",                 "Balcony"},
    {"Hermes",              "Cycle"},
    {"Nibbler",             "Day & Night"},
    {"Cubert",              "Hand with Reflecting Sphere"},
    {"Zapp",                "Drawing Hands"},
    {"Joey Mousepad",       "Ascending and Descending"}
};

const std::map<std::string_view, std::string_view> schema {
    { "mysql", "CREATE TABLE zild_t(id INTEGER AUTO_INCREMENT PRIMARY KEY, name VARCHAR(255), percent REAL, image BLOB, created_at TIMESTAMP);" },
    { "postgresql", "CREATE TABLE zild_t(id SERIAL PRIMARY KEY, name VARCHAR(255), percent REAL, image BYTEA, created_at TIMESTAMP);" },
    { "sqlite", "CREATE TABLE zild_t(id INTEGER PRIMARY KEY, name VARCHAR(255), percent REAL, image BLOB, created_at INTEGER);" },
    { "oracle", "CREATE TABLE zild_t(id NUMBER GENERATED AS IDENTITY, name VARCHAR(255), percent REAL, image BLOB, created_at TIMESTAMP);" }
};

const std::string_view help = R"(
Please enter a valid database connection URL and press ENTER
E.g. sqlite:///tmp/sqlite.db?synchronous=normal
E.g. mysql://localhost:3306/test?user=root&password=root
E.g. postgresql://localhost:5432/test?user=root&password=root
E.g. oracle://scott:tiger@localhost:1521/servicename
To exit, enter '.' on a single line

Connection URL> )";

[[nodiscard]] static std::string time2iso8601(const std::chrono::system_clock::time_point& time) {
    auto tt = std::chrono::system_clock::to_time_t(time);
    std::array<char, 30> buffer;
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&tt));
    return std::string(buffer.data());
}

[[nodiscard]] static double random_double_0_to_10() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 10.0);
    return std::round(dis(gen) * 100.0) / 100.0;
}

static void testCreateSchema(ConnectionPool& pool) {
    Connection con = pool.getConnection();
    try { con.execute("DROP TABLE zild_t;"); } catch (...) {}
    con.execute(std::string(schema.at(pool.getURL().protocol())));
}

static void testPrepared(ConnectionPool& pool) {
    Connection con = pool.getConnection();
    
    PreparedStatement prep = con.prepareStatement("INSERT INTO zild_t (name, percent, image, created_at) VALUES(?, ?, ?, ?);");
    
    con.beginTransaction();
    for (const auto& [name, image] : data) {
        prep.bindValues(name,
                        random_double_0_to_10(),
                        std::span<const std::byte>(reinterpret_cast<const std::byte*>(image.data()), image.size()),
                        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
                        );
        prep.execute();
    }
    
    //Instead of binding all values at once we can also bind values one-by-one
    prep.bind(1, "Jin Sakai");
    prep.bind(2, 10);
    std::string_view kanagawa = "\u795E\u5948\u5DDD\u6C96\u6D6A\u88CF";
    prep.bind(3, std::span<const std::byte>(reinterpret_cast<const std::byte*>(kanagawa.data()), kanagawa.size()));
    prep.bind(4, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    prep.execute();

    // If the number of values does not match statement placeholders an exception is thrown
    try {
        prep.bindValues("Sauron", 0.00);
        std::cout << "Test failed, did not get exception\n";
        std::exit(1);
    } catch (const sql_exception& e) { }
    
    // Implicit prepared statement. Any execute or executeQuery statement which
    // takes parameters are automatically translated into a prepared statement.
    // Here we also demonstrate how to set a SQL null value by using a nullptr
    con.execute("UPDATE zild_t SET image = ? WHERE id = ?", nullptr, 11);
    con.commit();
}

static void testQuery(ConnectionPool& pool) {
    Connection con = pool.getConnection();
    
    // Implicit prepared statement because of parameters
    ResultSet result = con.executeQuery("SELECT id, name, percent, image, created_at FROM zild_t WHERE id < ? ORDER BY id;", 100);
    
    result.setFetchSize(10); // Optionally set prefetched rows. Default is 100
    
    assert(result.columnCount() == 5);
    assert(result.columnName(1).value() == "id");
    
    while (result.next()) {
        int id = result.getInt(1);
        auto name = result.getString("name");
        double percent = result.getDouble("percent");
        auto blob = result.getBlob("image");
        auto created_at = time2iso8601(std::chrono::system_clock::from_time_t(result.getTimestamp("created_at")));
        
        std::cout << std::format("  {:<4}{:<15}{:<7.2f}{:<29}{}\n",
                                 id,
                                 name.value_or("null"),
                                 percent,
                                 blob ? std::string(reinterpret_cast<const char*>(blob->data()), blob->size()) : "null",
                                 created_at);
        
        // Assert that SQL null above was set
        if (id == 11) {
            assert(result.isNull(4));
        }
    }
}

static void testException(ConnectionPool& pool) {
    try {
        Connection con = pool.getConnection();
        PreparedStatement p = con.prepareStatement("invalid statement");
        p.execute();
        std::cout << "Test failed, did not get exception\n";
        std::exit(1);
    } catch (const sql_exception& e) { }
    
    try {
        Connection con = pool.getConnection();
        ResultSet r = con.executeQuery("invalid query");
        r.next();
        std::cout << "Test failed, did not get exception\n";
        std::exit(1);
    } catch (const sql_exception& e) { }
}

static void testAbortHandler(ConnectionPool& pool) {
    // Abort handler with lambda
    bool abortHandlerCalled = false;
    pool.setAbortHandler([&abortHandlerCalled](std::string_view error) {
        abortHandlerCalled = true;
    });
        
    // Calling Exception_reset() to ensure the call-stack does not
    // have any Exception_Frame attached so we can do a clean throw
    Exception_reset();

    // Throwing a libzdb exception without a TRY-block will cause the
    // abort handler to be called, iff set.
    THROW(SQLException, "SQLException");
    
    if (!abortHandlerCalled) {
        std::cout << "Test failed: Abort handler was not called\n";
        std::exit(1);
    }
    
    // Test Abort handler with another lambda
    std::string capturedError;
    pool.setAbortHandler([&capturedError](std::string_view error) {
        capturedError = error;
    });
    
    // Trigger the abort handler
    THROW(SQLException, "Another SQLException");
    
    // Verify the abort handler was called and captured the error
    if (capturedError.empty()) {
        std::cout << "Test failed: Abort handler was not called or did not capture error\n";
        std::exit(1);
    }
    
    // Reset abort handler
    pool.setAbortHandler();
}

static void testDropSchema(ConnectionPool& pool) {
    pool.getConnection().execute("DROP TABLE zild_t;");
}

int main() {
    
    std::cout << "\033[0;35m\nC++ zdbpp.h API Test:\033[0m\n" << help;
    
    for (std::string line; std::getline(std::cin, line);) {
        if (line == "q" || line == ".")
            break;
        
        std::optional<URL> url_opt;
        try {
            url_opt = URL(line);
        } catch(...) {
            std::cout << "Please enter a valid database URL or stop by entering '.'\n\n";
            std::cout << "Connection URL> ";
            continue;
        }
        
        ConnectionPool pool(std::move(*url_opt));
        pool.setReaper(0); // Disable reaper
        assert(pool.getType() > CONNECTIONPOOL_NONE);
        pool.start();
        std::cout << std::string(8, '=') + "> Start Tests\n";
        testCreateSchema(pool);
        testPrepared(pool);
        testQuery(pool);
        testException(pool);
        testAbortHandler(pool);
        testDropSchema(pool);
        std::cout << std::string(8, '=') + "> Tests: OK\n";
        std::cout << help;
    }
}
