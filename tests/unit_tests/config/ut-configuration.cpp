/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Michal Witanowski <m.witanowski@samsung.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */


/**
 * @file
 * @author  Michal Witanowski (m.witanowski@samsung.com)
 * @brief   Unit test of Configuration
 */

#include "config.hpp"
#include "ut.hpp"
#include "testconfig-example.hpp"
#include "config/manager.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace config;

BOOST_AUTO_TEST_SUITE(ConfigurationSuite)

// Floating point tolerance as a number of rounding errors
const int TOLERANCE = 1;


BOOST_AUTO_TEST_CASE(FromStringTest)
{
    TestConfig testConfig;

    BOOST_REQUIRE_NO_THROW(loadFromString(jsonTestString, testConfig));

    BOOST_CHECK_EQUAL(12345, testConfig.intVal);
    BOOST_CHECK_EQUAL(-1234567890123456789ll, testConfig.int64Val);
    BOOST_CHECK_EQUAL("blah", testConfig.stringVal);
    BOOST_CHECK_CLOSE(-1.234, testConfig.doubleVal, TOLERANCE);
    BOOST_CHECK_EQUAL(true, testConfig.boolVal);

    BOOST_REQUIRE_EQUAL(0, testConfig.emptyIntVector.size());

    BOOST_REQUIRE_EQUAL(3, testConfig.intVector.size());
    BOOST_CHECK_EQUAL(1, testConfig.intVector[0]);
    BOOST_CHECK_EQUAL(2, testConfig.intVector[1]);
    BOOST_CHECK_EQUAL(3, testConfig.intVector[2]);

    BOOST_REQUIRE_EQUAL(2, testConfig.stringVector.size());
    BOOST_CHECK_EQUAL("a", testConfig.stringVector[0]);
    BOOST_CHECK_EQUAL("b", testConfig.stringVector[1]);

    BOOST_REQUIRE_EQUAL(3, testConfig.doubleVector.size());
    BOOST_CHECK_CLOSE(0.0, testConfig.doubleVector[0], TOLERANCE);
    BOOST_CHECK_CLOSE(1.0, testConfig.doubleVector[1], TOLERANCE);
    BOOST_CHECK_CLOSE(2.0, testConfig.doubleVector[2], TOLERANCE);

    BOOST_CHECK_EQUAL(54321, testConfig.subObj.intVal);
    BOOST_CHECK_EQUAL(2,     testConfig.subObj.intVector.size());
    BOOST_CHECK_EQUAL(1,     testConfig.subObj.intVector[0]);
    BOOST_CHECK_EQUAL(2,     testConfig.subObj.intVector[1]);
    BOOST_CHECK_EQUAL(234,   testConfig.subObj.subSubObj.intVal);

    BOOST_REQUIRE_EQUAL(2, testConfig.subVector.size());
    BOOST_CHECK_EQUAL(123, testConfig.subVector[0].intVal);
    BOOST_CHECK_EQUAL(456, testConfig.subVector[1].intVal);
    BOOST_CHECK_EQUAL(345, testConfig.subVector[0].subSubObj.intVal);
    BOOST_CHECK_EQUAL(567, testConfig.subVector[1].subSubObj.intVal);
    BOOST_CHECK_EQUAL(3,   testConfig.subVector[0].intVector[0]);
    BOOST_CHECK_EQUAL(5,   testConfig.subVector[1].intVector[0]);
    BOOST_CHECK_EQUAL(4,   testConfig.subVector[0].intVector[1]);
    BOOST_CHECK_EQUAL(6,   testConfig.subVector[1].intVector[1]);

}


BOOST_AUTO_TEST_CASE(ToStringTest)
{
    TestConfig testConfig;
    BOOST_REQUIRE_NO_THROW(loadFromString(jsonTestString, testConfig));

    std::string out = saveToString(testConfig);
    BOOST_CHECK_EQUAL(out, jsonTestString);
}

namespace loadErrorsTest {

#define DECLARE_CONFIG(name, type) \
    struct name { \
        type field; \
        CONFIG_REGISTER(field) \
    };
DECLARE_CONFIG(IntConfig, int)
DECLARE_CONFIG(StringConfig, std::string)
DECLARE_CONFIG(DoubleConfig, double)
DECLARE_CONFIG(BoolConfig, bool)
DECLARE_CONFIG(ArrayConfig, std::vector<int>)
DECLARE_CONFIG(ObjectConfig, IntConfig)
#undef DECLARE_CONFIG

} // namespace loadErrorsTest

BOOST_AUTO_TEST_CASE(LoadErrorsTest)
{
    using namespace loadErrorsTest;

    IntConfig config;
    BOOST_REQUIRE_NO_THROW(loadFromString("{\"field\":1}", config));

    BOOST_CHECK_THROW(loadFromString("", config), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{", config), ConfigException); // invalid json
    BOOST_CHECK_THROW(loadFromString("{}", config), ConfigException); // missing field

    // invalid type

    IntConfig intConfig;
    BOOST_CHECK_NO_THROW(loadFromString("{\"field\": 1}", intConfig));
    BOOST_CHECK_THROW(loadFromString("{\"field\": \"1\"}", intConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1.0}", intConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": true}", intConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": []}", intConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": {}}", intConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1234567890123456789}", intConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": -1234567890123456789}", intConfig), ConfigException);

    StringConfig stringConfig;
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1}", stringConfig), ConfigException);
    BOOST_CHECK_NO_THROW(loadFromString("{\"field\": \"1\"}", stringConfig));
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1.0}", stringConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": true}", stringConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": []}", stringConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": {}}", stringConfig), ConfigException);

    DoubleConfig doubleConfig;
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1}", doubleConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": \"1\"}", doubleConfig), ConfigException);
    BOOST_CHECK_NO_THROW(loadFromString("{\"field\": 1.0}", doubleConfig));
    BOOST_CHECK_THROW(loadFromString("{\"field\": true}", doubleConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": []}", doubleConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": {}}", doubleConfig), ConfigException);

    BoolConfig boolConfig;
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1}", boolConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": \"1\"}", boolConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1.0}", boolConfig), ConfigException);
    BOOST_CHECK_NO_THROW(loadFromString("{\"field\": true}", boolConfig));
    BOOST_CHECK_THROW(loadFromString("{\"field\": []}", boolConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": {}}", boolConfig), ConfigException);

    ArrayConfig arrayConfig;
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1}", arrayConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": \"1\"}", arrayConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1.0}", arrayConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": true}", arrayConfig), ConfigException);
    BOOST_CHECK_NO_THROW(loadFromString("{\"field\": []}", arrayConfig));
    BOOST_CHECK_THROW(loadFromString("{\"field\": {}}", arrayConfig), ConfigException);

    ObjectConfig objectConfig;
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1}", objectConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": \"1\"}", objectConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": 1.0}", objectConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": true}", objectConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": []}", objectConfig), ConfigException);
    BOOST_CHECK_THROW(loadFromString("{\"field\": {}}", objectConfig), ConfigException);
    BOOST_CHECK_NO_THROW(loadFromString("{\"field\": {\"field\": 1}}", objectConfig));
}

namespace hasVisitableTest {

struct NotVisitable {};
struct Visitable {
    template<typename V>
    void accept(V v);
};
struct ConstVisitable {
    template<typename V>
    void accept(V v) const;
};
struct FullVisitable {
    template<typename V>
    void accept(V v);
    template<typename V>
    void accept(V v) const;
};
struct DerivedVisitable : FullVisitable {};
struct MissingArg {
    template<typename V>
    void accept();
};
struct WrongArg {
    template<typename V>
    void accept(int v);
};
struct NotFunction {
    int accept;
};

} // namespace hasVisitableTest

BOOST_AUTO_TEST_CASE(HasVisibleInternalHelperTest)
{
    using namespace hasVisitableTest;

    static_assert(isVisitable<Visitable>::value, "");
    static_assert(isVisitable<ConstVisitable>::value, "");
    static_assert(isVisitable<FullVisitable>::value, "");
    static_assert(isVisitable<DerivedVisitable>::value, "");

    static_assert(!isVisitable<NotVisitable>::value, "");
    static_assert(!isVisitable<MissingArg>::value, "");
    static_assert(!isVisitable<WrongArg>::value, "");
    static_assert(!isVisitable<NotFunction>::value, "");

    BOOST_CHECK(isVisitable<Visitable>());
}

BOOST_AUTO_TEST_CASE(FromToKVStoreTest)
{
    TestConfig config;
    loadFromString(jsonTestString, config);

    std::string dbPath = fs::unique_path("/tmp/kvstore-%%%%.db3").string();

    saveToKVStore(dbPath, config);
    TestConfig outConfig;
    loadFromKVStore(dbPath, outConfig);

    std::string out = saveToString(outConfig);
    BOOST_CHECK_EQUAL(out, jsonTestString);

    fs::remove(dbPath);
    fs::remove(dbPath + "-journal");
}

BOOST_AUTO_TEST_CASE(FromToFDTest)
{
    TestConfig config;
    loadFromString(jsonTestString, config);
    // Setup fd
    std::string fifoPath = fs::unique_path("/tmp/fdstore-%%%%").string();
    BOOST_CHECK(::mkfifo(fifoPath.c_str(), S_IWUSR | S_IRUSR) >= 0);
    int fd = ::open(fifoPath.c_str(), O_RDWR);
    BOOST_REQUIRE(fd >= 0);

    // The test
    saveToFD(fd, config);
    TestConfig outConfig;
    loadFromFD(fd, outConfig);
    std::string out = saveToString(outConfig);
    BOOST_CHECK_EQUAL(out, jsonTestString);

    // Cleanup
    BOOST_CHECK(::close(fd) >= 0);
    fs::remove(fifoPath);
}

BOOST_AUTO_TEST_CASE(ConfigUnionTest)
{
    TestConfig testConfig;
    BOOST_REQUIRE_NO_THROW(loadFromString(jsonTestString, testConfig));

    BOOST_CHECK(testConfig.union1.is<int>());
    BOOST_CHECK(!testConfig.union1.is<TestConfig::SubConfig>());
    BOOST_CHECK_EQUAL(testConfig.union1.as<int>(), 2);
    BOOST_CHECK(!testConfig.union2.is<int>());
    BOOST_CHECK(testConfig.union2.is<TestConfig::SubConfig>());
    TestConfig::SubConfig& subConfig = testConfig.union2.as<TestConfig::SubConfig>();
    BOOST_CHECK_EQUAL(subConfig.intVal, 54321);
    BOOST_CHECK(testConfig.unions[0].is<int>());
    BOOST_CHECK(testConfig.unions[1].is<TestConfig::SubConfig>());
    std::string out = saveToString(testConfig);
    BOOST_CHECK_EQUAL(out, jsonTestString);

    //Check copy

    std::vector<TestConfig::SubConfigOption> unions(2);
    unions[0].set<int>(2);
    //set from const lvalue reference (copy)
    unions[1].set(testConfig.unions[1].as<const TestConfig::SubConfig>());
    BOOST_CHECK(!testConfig.unions[1].as<TestConfig::SubConfig>().subSubObj.isMoved());
    //set from lvalue reference (copy)
    unions[1].set(testConfig.unions[1].as<TestConfig::SubConfig>());
    BOOST_CHECK(!testConfig.unions[1].as<TestConfig::SubConfig>().subSubObj.isMoved());
    //set from const rvalue reference (copy)
    unions[1].set(std::move(testConfig.unions[1].as<const TestConfig::SubConfig>()));
    BOOST_CHECK(!testConfig.unions[1].as<TestConfig::SubConfig>().subSubObj.isMoved());
    //set rvalue reference (copy -- move is disabled)
    unions[1].set(std::move(testConfig.unions[1].as<TestConfig::SubConfig>()));
    BOOST_CHECK(!testConfig.unions[1].as<TestConfig::SubConfig>().subSubObj.isMoved());
    //assign lvalue reference (copy)
    testConfig.unions[1] = unions[1];
    BOOST_CHECK(!unions[1].as<TestConfig::SubConfig>().subSubObj.isMoved());
    //assign rvalue reference (copy -- move is disabled)
    testConfig.unions[1] = std::move(unions[1]);
    BOOST_CHECK(!unions[1].as<TestConfig::SubConfig>().subSubObj.isMoved());

    testConfig.unions.clear();
    testConfig.unions = unions;
    out = saveToString(testConfig);
    BOOST_CHECK_EQUAL(out, jsonTestString);
}

BOOST_AUTO_TEST_SUITE_END()
