/**
 *  @file    jinja2Converter.cpp
 *
 *  @author  Tobias Anker <tobias.anker@kitsunemimi.moe>
 *
 *  @copyright MIT License
*/

#include "jinja2_converter.h"

#include <jinja2_parsing/jinja2_parser_interface.h>
#include <jinja2_items.h>

using Kitsune::Common::DataItem;
using Kitsune::Common::DataArray;
using Kitsune::Common::DataValue;
using Kitsune::Common::DataMap;

namespace Kitsune
{
namespace Jinja2
{

/**
 * @brief Iconstructor
 */
Jinja2Converter::Jinja2Converter(const bool traceParsing)
{
    m_driver = new Jinja2ParserInterface(traceParsing);
}

/**
 * @brief Idestructor which only deletes the parser-interface to avoid memory-lead
 */
Jinja2Converter::~Jinja2Converter()
{
    delete m_driver;
}

/**
 * @brief convert-method for the external using. At first it parse the template-string
 *        and then it merge the parsed information with the content of the json-input.
 *
 * @param templateString jinj2-formated string
 * @param input json-object with the information, which should be filled in the jinja2-template
 *
 * @return Pair of boolean and string where the boolean shows
 *         success: first is true and second contains the converted string
 *         failed: first is false and second contains the error-message
 */
std::pair<bool, std::string>
Jinja2Converter::convert(const std::string &templateString,
                         Common::DataMap* input)
{
    std::pair<bool, std::string> result;

    // parse jinja2-template into a json-tree
    result.first = m_driver->parse(templateString);

    // process a failure
    if(result.first == false)
    {
        result.second = m_driver->getErrorMessage();
        return result;
    }

    // convert the json-tree from the parser into a string
    // by filling the input into it
    Jinja2Item* output = m_driver->getOutput();
    if(output == nullptr) {
        return result;
    }

    result.first = processItem(input, output, &result.second);
    delete output;

    return result;
}

/**
 * @brief Process a json-array, which is a list of parsed parts of the jinja2-template
 *
 * @param input The json-object with the items, which sould be filled in the template
 * @param part The Jinja2Item with the jinja2-content
 *
 * @return true, if step was successful, else false
 */
bool
Jinja2Converter::processItem(Common::DataMap* input,
                             Jinja2Item* part,
                             std::string* output)
{
    if(part == nullptr) {
        return true;
    }

    //------------------------------------------------------
    if(part->getType() == Jinja2Item::TEXT_ITEM)
    {
        TextItem* textItem = dynamic_cast<TextItem*>(part);
        output->append(textItem->text);
        return processItem(input, part->next, output);
    }
    //------------------------------------------------------
    if(part->getType() == Jinja2Item::REPLACE_ITEM)
    {
        ReplaceItem* replaceItem = dynamic_cast<ReplaceItem*>(part);
        if(processReplace(input, replaceItem, output) == false) {
            return false;
        }
    }
    //------------------------------------------------------
    if(part->getType() == Jinja2Item::IF_ITEM)
    {
        IfItem* ifItem = dynamic_cast<IfItem*>(part);
        if(processIfCondition(input, ifItem, output) == false) {
            return false;
        }
    }
    //------------------------------------------------------
    if(part->getType() == Jinja2Item::FOR_ITEM)
    {
        ForLoopItem* forLoopItem = dynamic_cast<ForLoopItem*>(part);
        if(processForLoop(input, forLoopItem, output) == false) {
            return false;
        }
    }
    //------------------------------------------------------

    return true;
}

/**
 * @brief Resolve an replace-rule of the parsed jinja2-template
 *
 * @param input The json-object with the items, which sould be filled in the template
 * @param replaceObject ReplaceItem with the replacement-information
 * @param output Pointer to the output-string for the result of the convertion
 *
 * @return true, if step was successful, else false
 */
bool
Jinja2Converter::processReplace(Common::DataMap* input,
                                ReplaceItem* replaceObject,
                                std::string* output)
{
    // get information
    std::pair<std::string, bool> jsonValue = getString(input, &replaceObject->iterateArray);

    // process a failure
    if(jsonValue.second == false)
    {
        output->clear();
        output->append(createErrorMessage(&replaceObject->iterateArray));
        return false;
    }

    // insert the replacement
    output->append(jsonValue.first);

    return processItem(input, replaceObject->next, output);
}

/**
 * @brief Resolve an if-condition of the parsed jinja2-template
 *
 * @param input The json-object with the items, which sould be filled in the template
 * @param ifCondition Jinja2Item with the if-condition-information
 * @param output Pointer to the output-string for the result of the convertion
 *
 * @return true, if step was successful, else false
 */
bool
Jinja2Converter::processIfCondition(Common::DataMap* input,
                                    IfItem* ifCondition,
                                    std::string* output)
{
    // get information
    std::pair<std::string, bool> jsonValue = getString(input, &ifCondition->leftSide);

    // process a failure
    if(jsonValue.second == false)
    {
        output->clear();
        output->append(createErrorMessage(&ifCondition->leftSide));
        return false;
    }

    // run the if-condition of the jinja2-template
    if(jsonValue.first == ifCondition->rightSide.toString()
        || jsonValue.first == "True"
        || jsonValue.first == "true")
    {
        processItem(input, ifCondition->ifChild, output);
    }
    else
    {
        if(ifCondition->elseChild != nullptr) {
            processItem(input, ifCondition->elseChild, output);
        }
    }

    return processItem(input, ifCondition->next, output);
}

/**
 * @brief Resolve an for-loop of the parsed jinja2-template
 *
 * @param input The json-object with the items, which sould be filled in the template
 * @param forLoop ForLoopItem with the loop-information
 * @param output Pointer to the output-string for the result of the convertion
 *
 * @return true, if step was successful, else false
 */
bool
Jinja2Converter::processForLoop(Common::DataMap* input,
                                ForLoopItem* forLoop,
                                std::string* output)
{
    // get information
    std::pair<Common::DataItem*, bool> jsonValue = getItem(input, &forLoop->iterateArray);

    // process a failure
    if(jsonValue.second == false)
    {
        output->clear();
        output->append(createErrorMessage(&forLoop->iterateArray));
        return false;
    }

    // loop can only work on json-arrays
    if(jsonValue.first->getType() != Common::DataItem::ARRAY_TYPE) {
        return false;
    }

    // run the loop of the jinja2-template
    Common::DataArray* array = jsonValue.first->toArray();
    for(uint32_t i = 0; i < array->size(); i++)
    {
        Common::DataMap* tempLoopInput = input;
        tempLoopInput->insert(forLoop->tempVarName,
                              array->get(i), true);

        if(processItem(tempLoopInput, forLoop->forChild, output) == false) {
            return false;
        }
    }

    return processItem(input, forLoop->next, output);
}

/**
 * @brief Search a specific string-typed item in the json-input
 *
 * @param input The json-object in which the item sould be searched
 * @param jsonPath Path item in the json-object. It is a Common::DataArray
 *                 which contains only string-objects
 *
 * @return Pair of string and boolean where the boolean shows
 *         if the item was found and is a string-type
 *         and the string contains the item, if the search was successful
 */
std::pair<std::string, bool>
Jinja2Converter::getString(Common::DataMap* input,
                           Common::DataArray* jsonPath)
{
    // init
    std::pair<std::string, bool> result;
    result.second = false;

    // make a generic item-search and than try to convert to string
    std::pair<Common::DataItem*, bool> item = getItem(input, jsonPath);
    if(item.second == false) {
        return result;
    }

    if(item.first->toValue()->getValueType() == Common::DataItem::STRING_TYPE)
    {
        result.second = item.second;
        result.first = item.first->toValue()->getString();
    }

    if(item.first->toValue()->getValueType() == Common::DataItem::INT_TYPE)
    {
        result.second = item.second;
        const int intValue = item.first->toValue()->getInt();
        result.first = std::to_string(intValue);
    }

    return result;
}

/**
 * @brief Search a specific item in the json-input
 *
 * @param input The json-object in which the item sould be searched
 * @param jsonPath Path item in the json-object. It is a Common::DataArray
 *                 which contains only string-objects
 *
 * @return Pair of json-value and boolean where the boolean shows
 *         if the item was found and the json-value contains the item,
 *         if the search was successful
 */
std::pair<Common::DataItem*, bool>
Jinja2Converter::getItem(Common::DataMap* input,
                         Common::DataArray* jsonPath)
{
    // init
    std::pair<Common::DataItem*, bool> result;
    result.second = false;

    // search for the item
    Common::DataItem* tempJson = input;
    for(uint32_t i = 0; i < jsonPath->size(); i++)
    {
        tempJson = tempJson->get(jsonPath->get(i)->toString());
        if(tempJson == nullptr) {
            return result;
        }
    }
    result.second = true;
    result.first = tempJson;
    return result;
}

/**
 * @brief Is called, when an error occurs while compiling and generates an error-message for output
 *
 * @param jsonPath path within the json-object to the item which was not found
 *
 * @return error-messaage for the user
 */
std::string
Jinja2Converter::createErrorMessage(Common::DataArray* jsonPath)
{
    std::string errorMessage = "";
    errorMessage =  "error while converting jinja2-template \n";
    errorMessage += "    can not find item in path in json-input: ";

    // convert jsonPath into a string
    for(uint32_t i = 0; i < jsonPath->size(); i++)
    {
        if(i != 0) {
            errorMessage += ".";
        }
        //errorMessage.append(jsonPath->get(i));
    }

    errorMessage += "\n";
    errorMessage += "    or maybe the item does not have a valid format";
    errorMessage +=    " or the place where it should be used \n";

    return errorMessage;
}

}  // namespace Jinja2
}  // namespace Kitsune