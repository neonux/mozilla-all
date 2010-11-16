////////////////////////////////////////////////////////////////////////////////
// Public

const BOUNDARY_CHAR = nsIAccessibleText.BOUNDARY_CHAR;
const BOUNDARY_WORD_START = nsIAccessibleText.BOUNDARY_WORD_START;
const BOUNDARY_WORD_END = nsIAccessibleText.BOUNDARY_WORD_END;
const BOUNDARY_LINE_START = nsIAccessibleText.BOUNDARY_LINE_START;
const BOUNDARY_LINE_END = nsIAccessibleText.BOUNDARY_LINE_END;
const BOUNDARY_ATTRIBUTE_RANGE = nsIAccessibleText.BOUNDARY_ATTRIBUTE_RANGE;

const kTodo = 1;
const kOk = 2;

function testText(aIDs, aStartOffset, aEndOffset, aText)
{
  for (var i = 0; i < aIDs.length; i++)
  {
    var acc = getAccessible(aIDs[i], nsIAccessibleText);
    try {
      is(acc.getText(aStartOffset, aEndOffset), aText,
         "getText: wrong text between start and end offsets '" + aStartOffset +
         "', '" + aEndOffset + " for '" + prettyName(aIDs[i]) + "'");
    } catch (e) {
      ok(false,
         "getText fails between start and end offsets '" + aStartOffset +
         "', '" + aEndOffset + " for '" + prettyName(aIDs[i]) + "'");
    }
  }
}

/**
 * Test getTextAtOffset function over different elements
 *
 * @param aOffset         [in] the offset to get the text at
 * @param aBoundaryType   [in] Boundary type for text to be retrieved
 * @param aText           [in] expected return text for getTextAtOffset
 * @param aStartOffset    [in] expected return start offset for getTextAtOffset
 * @param aEndOffset      [in] expected return end offset for getTextAtOffset
 * @param ...             [in] list of tuples made of:
 *                              element identifier
 *                              kTodo or kOk for returned text
 *                              kTodo or kOk for returned start offset
 *                              kTodo or kOk for returned offset result
 *          
 */
function testTextAtOffset(aOffset, aBoundaryType, aText,
                          aStartOffset, aEndOffset)
{
  for (var i = 5; i < arguments.length; i = i + 4) {
    var ID = arguments[i];
    var acc = getAccessible(ID, nsIAccessibleText);
    var toDoFlag1 = arguments[i + 1];
    var toDoFlag2 = arguments[i + 2];
    var toDoFlag3 = arguments[i + 3];

    testTextHelper(ID, aOffset, aBoundaryType,
                   aText, aStartOffset, aEndOffset,
                   toDoFlag1, toDoFlag2, toDoFlag3,
                   acc.getTextAtOffset, "getTextAtOffset ");
  }
}

/**
 * Test getTextAfterOffset function over different elements
 *
 * @param aOffset         [in] the offset to get the text after
 * @param aBoundaryType   [in] Boundary type for text to be retrieved
 * @param aText           [in] expected return text for getTextAfterOffset
 * @param aStartOffset    [in] expected return start offset for getTextAfterOffset
 * @param aEndOffset      [in] expected return end offset for getTextAfterOffset
 * @param ...             [in] list of tuples made of:
 *                              element identifier
 *                              kTodo or kOk for returned text
 *                              kTodo or kOk for returned start offset
 *                              kTodo or kOk for returned offset result
 *          
 */
function testTextAfterOffset(aOffset, aBoundaryType,
                             aText, aStartOffset, aEndOffset)
{
  for (var i = 5; i < arguments.length; i = i + 4) {
    var ID = arguments[i];
    var acc = getAccessible(ID, nsIAccessibleText);
    var toDoFlag1 = arguments[i + 1];
    var toDoFlag2 = arguments[i + 2];
    var toDoFlag3 = arguments[i + 3];

    testTextHelper(ID, aOffset, aBoundaryType,
                   aText, aStartOffset, aEndOffset,
                   toDoFlag1, toDoFlag2, toDoFlag3, 
                   acc.getTextAfterOffset, "getTextAfterOffset ");
  }
}

/**
 * Test getTextBeforeOffset function over different elements
 *
 * @param aOffset         [in] the offset to get the text before
 * @param aBoundaryType   [in] Boundary type for text to be retrieved
 * @param aText           [in] expected return text for getTextBeforeOffset
 * @param aStartOffset    [in] expected return start offset for getTextBeforeOffset
 * @param aEndOffset      [in] expected return end offset for getTextBeforeOffset
 * @param ...             [in] list of tuples made of:
 *                              element identifier
 *                              kTodo or kOk for returned text
 *                              kTodo or kOk for returned start offset
 *                              kTodo or kOk for returned offset result
 *          
 */
function testTextBeforeOffset(aOffset, aBoundaryType,
                              aText, aStartOffset, aEndOffset)
{
  for (var i = 5; i < arguments.length; i = i + 4) {
    var ID = arguments[i];
    var acc = getAccessible(ID, nsIAccessibleText);
    var toDoFlag1 = arguments[i + 1];
    var toDoFlag2 = arguments[i + 2];
    var toDoFlag3 = arguments[i + 3];

    testTextHelper(ID, aOffset, aBoundaryType,
                   aText, aStartOffset, aEndOffset,
                   toDoFlag1, toDoFlag2, toDoFlag3,
                   acc.getTextBeforeOffset, "getTextBeforeOffset ");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Private

function testTextHelper(aID, aOffset, aBoundaryType,
                        aText, aStartOffset, aEndOffset,
                        aToDoFlag1, aToDoFlag2, aToDoFlag3,
                        aTextFunc, aTextFuncName)
{
  var exceptionFlag = aToDoFlag1 == undefined ||
                      aToDoFlag2 == undefined ||
                      aToDoFlag3 == undefined;
  try {
    var startOffsetObj = {}, endOffsetObj = {};
    var text = aTextFunc(aOffset, aBoundaryType,
                         startOffsetObj, endOffsetObj);
    
    var isFunc1 = (aToDoFlag1 == kTodo) ? todo_is : is;
    var isFunc2 = (aToDoFlag2 == kTodo) ? todo_is : is;
    var isFunc3 = (aToDoFlag3 == kTodo) ? todo_is : is;

    var startMsg = aTextFuncName + "(" + boundaryToString(aBoundaryType) + "): ";

    var endMsg = ", id: '" + prettyName(aID) + "';";
    
    isFunc1(text, aText,
            startMsg + "wrong text, offset: " + aOffset + endMsg);
    isFunc2(startOffsetObj.value, aStartOffset,
            startMsg + "wrong start offset, offset: " + aOffset + endMsg);
    isFunc3(endOffsetObj.value, aEndOffset,
            startMsg + "wrong end offset, offset: " + aOffset + endMsg);
    
  } catch (e) {
    var okFunc = exceptionFlag ? todo : ok;
    okFunc(false, startMsg + "failed at offset " + aOffset + endMsg);
  }
}

function boundaryToString(aBoundaryType)
{
  switch (aBoundaryType) {
    case BOUNDARY_CHAR:
      return "char";
    case BOUNDARY_WORD_START:
      return "word start";
    case BOUNDARY_WORD_END:
      return "word end";
    case BOUNDARY_LINE_START:
      return "line start";
    case BOUNDARY_LINE_END:
      return "line end";
    case BOUNDARY_ATTRIBUTE_RANGE:
      return "attr range";
  }
}
