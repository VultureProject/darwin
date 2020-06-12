#include <iostream>

extern "C" {
#include <yara.h>
}

#include "catch2/catch.hpp"
#include "rapidjson/document.h"
#include "Yara.hpp"

namespace darwin {

    namespace tests {

        SCENARIO( "testing Yara compiler", "[toolkit][yara]" ) {

            GIVEN( "an initialised compiler" ) {
                toolkit::YaraCompiler compiler;
                REQUIRE(compiler.Init());

                WHEN("no file is provided") {
                    THEN("compiler has no rules") {
                        REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NO_RULES);
                    }
                    THEN("compiler can't compile rules") {
                        REQUIRE(not compiler.CompileRules());
                        AND_THEN("status stays unchanged") {
                            REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NO_RULES);
                        }
                    }
                    THEN("compiler can't provide an engine") {
                        REQUIRE(compiler.GetEngine() == nullptr);
                        AND_THEN("status stays unchanged") {
                            REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NO_RULES);
                        }
                    }
                }

                WHEN("no valid file is provided") {
                    THEN("compiler can't add file") {
                        REQUIRE(not compiler.AddRuleFile(nullptr, "namespace", "filename"));

                        AND_THEN("compiler has no rules") {
                            REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NO_RULES);
                        }
                        AND_THEN("compiler can't compile rules") {
                            REQUIRE(not compiler.CompileRules());
                            AND_THEN("status stays unchanged") {
                                    REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NO_RULES);
                                }
                        }
                        AND_THEN("compiler can't provide an engine") {
                            REQUIRE(compiler.GetEngine() == nullptr);
                            AND_THEN("status stays unchanged") {
                                REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NO_RULES);
                            }
                        }
                    }
                }

                WHEN("a valid file is provided, but the rule is not valid") {
                    std::string fileContent("rule eicar\n"
                        "{\n"
                            "   meta:\n"
                            "       description = \"This is not a valid rule\"\n"
                            "   strings:\n"
                                "       $s1 = \"X5O!P%@AP[4\\\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*\" fullword ascii\n"
                            "   condition:\n"
                                "       all of them\n"
                            "   this could be valid... But no"
                        "}\n");
                    {
                        FILE* file = fmemopen((void*)fileContent.c_str(), fileContent.size(), "r");
                        REQUIRE(file != nullptr);

                        THEN("compiler can't add rule") {
                            REQUIRE(not compiler.AddRuleFile(file, "", ""));

                            AND_THEN("compiler becomes unclean") {
                                REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::ERROR);
                            }
                            AND_THEN("compiler can't compile rules") {
                                REQUIRE(not compiler.CompileRules());
                            }
                            AND_THEN("compiler can't provide an engine") {
                                REQUIRE(compiler.GetEngine() == nullptr);
                            }

                            AND_WHEN("a new file is provided, with a valid rule this time") {
                                std::string fileContent("rule eicar\n"
                                    "{\n"
                                        "   meta:\n"
                                        "       description = \"This is a valid rule\"\n"
                                        "   strings:\n"
                                            "       $s1 = \"X5O!P%@AP[4\\\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*\" fullword ascii\n"
                                        "   condition:\n"
                                            "       all of them\n"
                                    "}\n");
                                {
                                    FILE* file = fmemopen((void*)fileContent.c_str(), fileContent.size(), "r");
                                    REQUIRE(file != nullptr);
                                    REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::ERROR);

                                    THEN("compiler still can't add new rules") {
                                        REQUIRE(not compiler.AddRuleFile(file, "", ""));

                                        AND_THEN("compiler is still unclean") {
                                            REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::ERROR);
                                        }
                                        AND_THEN("compiler still can't compile rules") {
                                            REQUIRE(not compiler.CompileRules());
                                        }
                                        AND_THEN("compiler still can't provide an engine") {
                                            REQUIRE(compiler.GetEngine() == nullptr);
                                        }
                                    }
                                }
                            }
                        }
                    }

                }

                WHEN("a new file is provided, with a valid rule") {
                    std::string fileContent("rule rule1\n"
                        "{\n"
                            "   meta:\n"
                            "       description = \"This is a valid rule\"\n"
                            "   strings:\n"
                            "       $s1 = \"rule1\" fullword ascii\n"
                            "   condition:\n"
                            "       all of them\n"
                        "}\n");
                    { // file's scope
                        FILE* file = fmemopen((void*)fileContent.c_str(), fileContent.size(), "r");
                        REQUIRE(file != nullptr);
                        THEN("compiler can add the rule") {
                            REQUIRE(compiler.AddRuleFile(file, "", ""));

                            AND_THEN("compiler's state has changed to 'NEW_RULES'") {
                                REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NEW_RULES);

                                AND_THEN("compiler can compile rules") {
                                    REQUIRE(compiler.CompileRules());

                                    AND_THEN("compiler can provide an engine") {
                                        REQUIRE(compiler.GetEngine() != nullptr);
                                    }
                                }
                                AND_THEN("compiler still can't provide an engine") {
                                    REQUIRE(compiler.GetEngine() == nullptr);

                                    AND_THEN("state has not changed") {
                                        REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NEW_RULES);
                                    }
                                    AND_THEN("compiler can compile rules") {
                                        REQUIRE(compiler.CompileRules());

                                        AND_THEN("compiler can provide an engine") {
                                            REQUIRE(compiler.GetEngine() != nullptr);
                                        }
                                    }
                                }
                            }

                            AND_WHEN("a new rule is added") {
                                std::string fileContent2("rule rule2\n"
                                    "{\n"
                                        "   meta:\n"
                                        "       description = \"This is another valid rule\"\n"
                                        "   strings:\n"
                                        "       $s1 = \"rule2\" fullword ascii\n"
                                        "   condition:\n"
                                        "       all of them\n"
                                    "}\n");
                                { // file2's scope
                                    FILE* file2 = fmemopen((void*)fileContent2.c_str(), fileContent2.size(), "r");
                                    REQUIRE(file2 != nullptr);
                                    THEN("compiler can add the rule") {
                                        REQUIRE(compiler.AddRuleFile(file2, "", ""));

                                        AND_THEN("compiler's state stays to 'NEW_RULES'") {
                                            REQUIRE(compiler.GetStatus() == toolkit::YaraCompiler::Status::NEW_RULES);

                                            AND_THEN("compiler can compile rules") {
                                                REQUIRE(compiler.CompileRules());

                                                AND_THEN("compiler can provide an engine") {
                                                    REQUIRE(compiler.GetEngine() != nullptr);
                                                }
                                            }
                                        }
                                    }
                                } // file2's end of scope
                            }
                        }
                    } // file's scope
                }
            } // given
        } // scenario

        SCENARIO("testing Yara engine", "[toolkit][yara]") {
            GIVEN("an uninitialised engine") {
                toolkit::YaraEngine engine;
                WHEN("the engine has to scan") {
                    std::string dataStr = "this is just a string";
                    const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(dataStr.c_str());
                    std::vector<unsigned char> data(raw_decoded, raw_decoded + dataStr.size());
                    REQUIRE(dataStr.size() == data.size());
                    THEN("the engine cannot scan") {
                        unsigned int certitude = 0;
                        REQUIRE(engine.ScanData(data, certitude) == -1);
                        CHECK(certitude == 0);
                        AND_THEN("results can be asked, but return an empty document") {
                            rapidjson::Document ret = engine.GetResults();
                            REQUIRE(ret.IsNull());
                        }
                    }

                    THEN("results can be asked, but return an empty document") {
                        rapidjson::Document ret = engine.GetResults();
                        REQUIRE(ret.IsNull());
                    }
                }
            }
            GIVEN("an initialised engine, with one rule") {
                toolkit::YaraCompiler compiler;
                std::shared_ptr<toolkit::YaraEngine> engine = nullptr;
                REQUIRE(compiler.Init());
                std::string fileContent("rule rule1: tag1\n"
                    "{\n"
                        "   meta:\n"
                        "       description = \"This is a valid rule\"\n"
                        "   strings:\n"
                        "       $s1 = \"rule1\" fullword ascii\n"
                        "   condition:\n"
                        "       all of them\n"
                    "}\n");
                { // file's scope
                    FILE* file = fmemopen((void*)fileContent.c_str(), fileContent.size(), "r");
                    REQUIRE(file != nullptr);
                    REQUIRE(compiler.AddRuleFile(file, "", ""));
                } // file's end of scope
                
                REQUIRE(compiler.CompileRules());
                engine = compiler.GetEngine();
                REQUIRE(engine != nullptr);

                WHEN("the engine has to scan data that doesn't match") {
                    std::string dataStr = "this is just a string";
                    const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(dataStr.c_str());
                    std::vector<unsigned char> data(raw_decoded, raw_decoded + dataStr.size());
                    REQUIRE(dataStr.size() == data.size());

                    THEN("the engine can scan, and tells us there is no match") {
                        unsigned int certitude = 10;
                        CHECK(engine->ScanData(data, certitude) == 0);
                        REQUIRE(certitude == 0);

                        AND_THEN("results can be asked, and are null") {
                            rapidjson::Document ret = engine->GetResults();
                            REQUIRE(ret.IsNull());
                        }
                    }
                }

                WHEN("the engine has to scan data that matches") {
                    std::string dataStr = "this is just a string, but with a matching rule1 inside";
                    const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(dataStr.c_str());
                    std::vector<unsigned char> data(raw_decoded, raw_decoded + dataStr.size());
                    REQUIRE(dataStr.size() == data.size());

                    THEN("the engine can scan, and tells us there is a match") {
                        unsigned int certitude = 0;
                        CHECK(engine->ScanData(data, certitude) == 1);
                        REQUIRE(certitude == 100);

                        AND_THEN("results can be asked, and contain the match") {
                            rapidjson::Document ret = engine->GetResults();
                            REQUIRE_FALSE(ret.IsNull());
                            REQUIRE(ret.IsArray());
                            REQUIRE(ret.Size() == 1);

                            auto array = ret.GetArray();
                            REQUIRE(array[0].IsObject());
                            auto obj = array[0].GetObject();

                            AND_THEN("the match contains a 'rule' string field") {
                                REQUIRE(obj.HasMember("rule"));

                                AND_THEN("the rule is a string, and is equal to 'rule1'") {
                                    REQUIRE(obj["rule"].IsString());
                                    REQUIRE_THAT(obj["rule"].GetString(), Catch::Matchers::Equals("rule1"));
                                }
                            }

                            AND_THEN("the match contains a tag array, with one object") {
                                REQUIRE(obj.HasMember("tags"));
                                REQUIRE(obj["tags"].IsArray());
                                REQUIRE(obj["tags"].Size() == 1);

                                AND_THEN("the tag contained is a tring, and is equal to 'tag1'") {
                                    REQUIRE(obj["tags"][0].IsString());
                                    REQUIRE_THAT(obj["tags"][0].GetString(), Catch::Matchers::Equals("tag1"));
                                }
                            }
                        }

                        AND_THEN("another scan can be started, and the result is correct") {
                            std::string dataStr2 = "this is just a string, but with no matching element inside";
                            const unsigned char* raw_decoded2 = reinterpret_cast<const unsigned char *>(dataStr2.c_str());
                            std::vector<unsigned char> data2(raw_decoded2, raw_decoded2 + dataStr2.size());
                            REQUIRE(dataStr2.size() == data2.size());

                            AND_THEN("the engine can scan, and tells us there is no match") {
                                unsigned int certitude = 10;
                                CHECK(engine->ScanData(data2, certitude) == 0);
                                REQUIRE(certitude == 0);

                                AND_THEN("results can be asked, and contain nothing") {
                                    rapidjson::Document ret2 = engine->GetResults();
                                    REQUIRE(ret2.IsNull());
                                }
                            }
                        }
                    }
                }
            } // given

            GIVEN("an initialised engine, with several rules") {
                toolkit::YaraCompiler compiler;
                std::shared_ptr<toolkit::YaraEngine> engine = nullptr;
                REQUIRE(compiler.Init());
                std::string fileContent("rule rule1: tag1\n"
                    "{\n"
                        "   meta:\n"
                        "       description = \"This is a valid rule\"\n"
                        "   strings:\n"
                        "       $s1 = \"test\" fullword ascii\n"
                        "   condition:\n"
                        "       all of them\n"
                    "}\n\n"
                    "rule rule2: tag2 tag3\n"
                    "{\n"
                        "   meta:\n"
                        "       description = \"This is another valid rule\"\n"
                        "   strings:\n"
                        "       $reg1 = /^hello (world|monde).*/ nocase\n"
                        "   condition:\n"
                        "       all of them\n"
                    "}\n\n");
                { // file's scope
                    FILE* file = fmemopen((void*)fileContent.c_str(), fileContent.size(), "r");
                    REQUIRE(file != nullptr);
                    REQUIRE(compiler.AddRuleFile(file, "", ""));
                } // file's end of scope
                
                REQUIRE(compiler.CompileRules());
                engine = compiler.GetEngine();
                REQUIRE(engine != nullptr);

                WHEN("the engine has to scan data that matches") {
                    std::string dataStr = "Hello world! this is a simple test for multi-rules match!";
                    const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(dataStr.c_str());
                    std::vector<unsigned char> data(raw_decoded, raw_decoded + dataStr.size());
                    REQUIRE(dataStr.size() == data.size());

                    THEN("the engine can scan, and tells us there is a match") {
                        unsigned int certitude = 0;
                        CHECK(engine->ScanData(data, certitude) == 1);
                        REQUIRE(certitude == 100);

                        AND_THEN("results can be asked, and contain the 2 matches") {
                            rapidjson::Document ret = engine->GetResults();
                            REQUIRE_FALSE(ret.IsNull());
                            REQUIRE(ret.IsArray());
                            REQUIRE(ret.Size() == 2);

                            auto array = ret.GetArray();
                            REQUIRE(array[0].IsObject());
                            auto obj = array[0].GetObject();

                            AND_THEN("the first match contains a 'rule' string field") {
                                REQUIRE(obj.HasMember("rule"));

                                AND_THEN("the rule is a string, and is equal to 'rule1'") {
                                    REQUIRE(obj["rule"].IsString());
                                    REQUIRE_THAT(obj["rule"].GetString(), Catch::Matchers::Equals("rule1"));
                                }
                            }

                            AND_THEN("the first match contains a tag array, with one object") {
                                REQUIRE(obj.HasMember("tags"));
                                REQUIRE(obj["tags"].IsArray());
                                REQUIRE(obj["tags"].Size() == 1);

                                AND_THEN("the tag contained is a string, and is equal to 'tag1'") {
                                    REQUIRE(obj["tags"][0].IsString());
                                    REQUIRE_THAT(obj["tags"][0].GetString(), Catch::Matchers::Equals("tag1"));
                                }
                            }

                            REQUIRE(array[1].IsObject());
                            auto obj2 = array[1].GetObject();

                            AND_THEN("the second match contains a 'rule' string field") {
                                REQUIRE(obj2.HasMember("rule"));

                                AND_THEN("the rule is a string, and is equal to 'rule2'") {
                                    REQUIRE(obj2["rule"].IsString());
                                    REQUIRE_THAT(obj2["rule"].GetString(), Catch::Matchers::Equals("rule2"));
                                }
                            }

                            AND_THEN("the second match contains a tag array, with two objects") {
                                REQUIRE(obj2.HasMember("tags"));
                                REQUIRE(obj2["tags"].IsArray());
                                REQUIRE(obj2["tags"].Size() == 2);

                                AND_THEN("the first tag is a string, and is equal to 'tag2'") {
                                    REQUIRE(obj2["tags"][0].IsString());
                                    REQUIRE_THAT(obj2["tags"][0].GetString(), Catch::Matchers::Equals("tag2"));
                                }

                                AND_THEN("the second tag is a string, and is equal to 'tag3'") {
                                    REQUIRE(obj2["tags"][1].IsString());
                                    REQUIRE_THAT(obj2["tags"][1].GetString(), Catch::Matchers::Equals("tag3"));
                                }
                            }
                        }
                    }
                }
            } // given
        } // scenario
    } // namespace tests
} // namespace darwin