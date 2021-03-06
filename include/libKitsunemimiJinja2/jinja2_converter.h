/**
 *  @file    jinja2Converter.h
 *
 *  @author  Tobias Anker <tobias.anker@kitsunemimi.moe>
 *
 *  @copyright MIT License
 */

#ifndef JINJA2CONVERTER_H
#define JINJA2CONVERTER_H

#include <utility>
#include <string>
#include <mutex>
#include <libKitsunemimiCommon/common_items/data_items.h>

namespace Kitsunemimi
{
namespace Jinja2
{
class Jinja2ParserInterface;
class Jinja2Item;
class ReplaceItem;
class IfItem;
class ForLoopItem;

class Jinja2Converter
{
public:
    static Jinja2Converter* getInstance();

    ~Jinja2Converter();

    bool convert(std::string &result,
                 const std::string &templateString,
                 const std::string &jsonInput,
                 std::string &errorMessage);

    bool convert(std::string &result,
                 const std::string &templateString,
                 DataMap* input,
                 std::string &errorMessage);

private:
    Jinja2Converter(const bool traceParsing = false);

    static Jinja2Converter* m_instance;

    Jinja2ParserInterface* m_driver = nullptr;
    std::mutex m_lock;

    bool processItem(DataMap* input,
                     Kitsunemimi::Jinja2::Jinja2Item* part,
                     std::string &output,
                     std::string &errorMessage);
    bool processReplace(DataMap* input,
                        ReplaceItem* replaceObject,
                        std::string &output,
                        std::string &errorMessage);
    bool processIfCondition(DataMap* input,
                            IfItem* ifCondition,
                            std::string &output,
                            std::string &errorMessage);
    bool processForLoop(DataMap* input,
                        ForLoopItem* forLoop,
                        std::string &output,
                        std::string &errorMessage);

    const std::pair<std::string, bool> getString(DataMap* input,
                                                 DataArray* jsonPath);
    const std::pair<DataItem*, bool> getItem(DataMap* input,
                                             DataArray* jsonPath);

    const std::string createErrorMessage(DataArray* jsonPath);
};

}  // namespace Jinja2
}  // namespace Kitsunemimi

#endif // JINJA2CONVERTER_H
