/*
 * Copyright (c) 2005-2007 Henri Sivonen
 * Copyright (c) 2007-2010 Mozilla Foundation
 * Portions of comments Copyright 2004-2010 Apple Computer, Inc., Mozilla 
 * Foundation, and Opera Software ASA.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * The comments following this one that use the same comment syntax as this 
 * comment are quotes from the WHATWG HTML 5 spec as of 2 June 2007 
 * amended as of June 18 2008 and May 31 2010.
 * That document came with this statement:
 * "© Copyright 2004-2010 Apple Computer, Inc., Mozilla Foundation, and 
 * Opera Software ASA. You are granted a license to use, reproduce and 
 * create derivative works of this document."
 */

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.Const;
import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NoLength;
import nu.validator.htmlparser.common.EncodingDeclarationHandler;
import nu.validator.htmlparser.common.Interner;
import nu.validator.htmlparser.common.TokenHandler;
import nu.validator.htmlparser.common.XmlViolationPolicy;

import org.xml.sax.ErrorHandler;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

/**
 * An implementation of
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html
 * 
 * This class implements the <code>Locator</code> interface. This is not an
 * incidental implementation detail: Users of this class are encouraged to make
 * use of the <code>Locator</code> nature.
 * 
 * By default, the tokenizer may report data that XML 1.0 bans. The tokenizer
 * can be configured to treat these conditions as fatal or to coerce the infoset
 * to something that XML 1.0 allows.
 * 
 * @version $Id$
 * @author hsivonen
 */
public class Tokenizer implements Locator {

    private static final int DATA_AND_RCDATA_MASK = ~1;

    public static final int DATA = 0;

    public static final int RCDATA = 1;

    public static final int SCRIPT_DATA = 2;

    public static final int PLAINTEXT = 3;

    private static final int TAG_OPEN = 4;

    private static final int CLOSE_TAG_OPEN = 5;

    private static final int TAG_NAME = 6;

    private static final int BEFORE_ATTRIBUTE_NAME = 7;

    private static final int ATTRIBUTE_NAME = 8;

    private static final int AFTER_ATTRIBUTE_NAME = 9;

    private static final int BEFORE_ATTRIBUTE_VALUE = 10;

    private static final int ATTRIBUTE_VALUE_DOUBLE_QUOTED = 11;

    private static final int ATTRIBUTE_VALUE_SINGLE_QUOTED = 12;

    private static final int ATTRIBUTE_VALUE_UNQUOTED = 13;

    private static final int AFTER_ATTRIBUTE_VALUE_QUOTED = 14;

    private static final int BOGUS_COMMENT = 15;

    private static final int MARKUP_DECLARATION_OPEN = 16;

    private static final int DOCTYPE = 17;

    private static final int BEFORE_DOCTYPE_NAME = 18;

    private static final int DOCTYPE_NAME = 19;

    private static final int AFTER_DOCTYPE_NAME = 20;

    private static final int BEFORE_DOCTYPE_PUBLIC_IDENTIFIER = 21;

    private static final int DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED = 22;

    private static final int DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED = 23;

    private static final int AFTER_DOCTYPE_PUBLIC_IDENTIFIER = 24;

    private static final int BEFORE_DOCTYPE_SYSTEM_IDENTIFIER = 25;

    private static final int DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED = 26;

    private static final int DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED = 27;

    private static final int AFTER_DOCTYPE_SYSTEM_IDENTIFIER = 28;

    private static final int BOGUS_DOCTYPE = 29;

    private static final int COMMENT_START = 30;

    private static final int COMMENT_START_DASH = 31;

    private static final int COMMENT = 32;

    private static final int COMMENT_END_DASH = 33;

    private static final int COMMENT_END = 34;

    private static final int COMMENT_END_SPACE = 35;

    private static final int COMMENT_END_BANG = 36;

    private static final int NON_DATA_END_TAG_NAME = 37;

    private static final int MARKUP_DECLARATION_HYPHEN = 38;

    private static final int MARKUP_DECLARATION_OCTYPE = 39;

    private static final int DOCTYPE_UBLIC = 40;

    private static final int DOCTYPE_YSTEM = 41;

    private static final int CONSUME_CHARACTER_REFERENCE = 42;

    private static final int CONSUME_NCR = 43;

    private static final int CHARACTER_REFERENCE_TAIL = 44;

    private static final int HEX_NCR_LOOP = 45;

    private static final int DECIMAL_NRC_LOOP = 46;

    private static final int HANDLE_NCR_VALUE = 47;

    private static final int SELF_CLOSING_START_TAG = 48;

    private static final int CDATA_START = 49;

    private static final int CDATA_SECTION = 50;

    private static final int CDATA_RSQB = 51;

    private static final int CDATA_RSQB_RSQB = 52;

    private static final int SCRIPT_DATA_LESS_THAN_SIGN = 53;

    private static final int SCRIPT_DATA_ESCAPE_START = 54;

    private static final int SCRIPT_DATA_ESCAPE_START_DASH = 55;

    private static final int SCRIPT_DATA_ESCAPED = 56;

    private static final int SCRIPT_DATA_ESCAPED_DASH = 57;

    private static final int SCRIPT_DATA_ESCAPED_DASH_DASH = 58;

    private static final int BOGUS_COMMENT_HYPHEN = 59;

    public static final int RAWTEXT = 60;

    private static final int RAWTEXT_RCDATA_LESS_THAN_SIGN = 61;

    private static final int AFTER_DOCTYPE_PUBLIC_KEYWORD = 62;

    private static final int BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS = 63;

    private static final int AFTER_DOCTYPE_SYSTEM_KEYWORD = 64;

    private static final int SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN = 65;

    private static final int SCRIPT_DATA_DOUBLE_ESCAPE_START = 66;

    private static final int SCRIPT_DATA_DOUBLE_ESCAPED = 67;

    private static final int SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN = 68;

    private static final int SCRIPT_DATA_DOUBLE_ESCAPED_DASH = 69;

    private static final int SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH = 70;

    private static final int SCRIPT_DATA_DOUBLE_ESCAPE_END = 71;

    private static final int CHARACTER_REFERENCE_HILO_LOOKUP = 72;

    /**
     * Magic value for UTF-16 operations.
     */
    private static final int LEAD_OFFSET = (0xD800 - (0x10000 >> 10));

    /**
     * UTF-16 code unit array containing less than and greater than for emitting
     * those characters on certain parse errors.
     */
    private static final @NoLength char[] LT_GT = { '<', '>' };

    /**
     * UTF-16 code unit array containing less than and solidus for emitting
     * those characters on certain parse errors.
     */
    private static final @NoLength char[] LT_SOLIDUS = { '<', '/' };

    /**
     * UTF-16 code unit array containing ]] for emitting those characters on
     * state transitions.
     */
    private static final @NoLength char[] RSQB_RSQB = { ']', ']' };

    /**
     * Array version of U+FFFD.
     */
    private static final @NoLength char[] REPLACEMENT_CHARACTER = { '\uFFFD' };

    // [NOCPP[

    /**
     * Array version of space.
     */
    private static final @NoLength char[] SPACE = { ' ' };

    // ]NOCPP]

    /**
     * Array version of line feed.
     */
    private static final @NoLength char[] LF = { '\n' };

    /**
     * Buffer growth parameter.
     */
    private static final int BUFFER_GROW_BY = 1024;

    /**
     * "CDATA[" as <code>char[]</code>
     */
    private static final @NoLength char[] CDATA_LSQB = "CDATA[".toCharArray();

    /**
     * "octype" as <code>char[]</code>
     */
    private static final @NoLength char[] OCTYPE = "octype".toCharArray();

    /**
     * "ublic" as <code>char[]</code>
     */
    private static final @NoLength char[] UBLIC = "ublic".toCharArray();

    /**
     * "ystem" as <code>char[]</code>
     */
    private static final @NoLength char[] YSTEM = "ystem".toCharArray();

    private static final char[] TITLE_ARR = { 't', 'i', 't', 'l', 'e' };

    private static final char[] SCRIPT_ARR = { 's', 'c', 'r', 'i', 'p', 't' };

    private static final char[] STYLE_ARR = { 's', 't', 'y', 'l', 'e' };

    private static final char[] PLAINTEXT_ARR = { 'p', 'l', 'a', 'i', 'n', 't',
            'e', 'x', 't' };

    private static final char[] XMP_ARR = { 'x', 'm', 'p' };

    private static final char[] TEXTAREA_ARR = { 't', 'e', 'x', 't', 'a', 'r',
            'e', 'a' };

    private static final char[] IFRAME_ARR = { 'i', 'f', 'r', 'a', 'm', 'e' };

    private static final char[] NOEMBED_ARR = { 'n', 'o', 'e', 'm', 'b', 'e',
            'd' };

    private static final char[] NOSCRIPT_ARR = { 'n', 'o', 's', 'c', 'r', 'i',
            'p', 't' };

    private static final char[] NOFRAMES_ARR = { 'n', 'o', 'f', 'r', 'a', 'm',
            'e', 's' };

    /**
     * The token handler.
     */
    protected final TokenHandler tokenHandler;

    protected EncodingDeclarationHandler encodingDeclarationHandler;

    // [NOCPP[

    /**
     * The error handler.
     */
    protected ErrorHandler errorHandler;

    // ]NOCPP]

    /**
     * Whether the previous char read was CR.
     */
    protected boolean lastCR;

    protected int stateSave;

    private int returnStateSave;

    protected int index;

    private boolean forceQuirks;

    private char additional;

    private int entCol;

    private int firstCharKey;

    private int lo;

    private int hi;

    private int candidate;

    private int strBufMark;

    private int prevValue;

    protected int value;

    private boolean seenDigits;

    protected int cstart;

    /**
     * The SAX public id for the resource being tokenized. (Only passed to back
     * as part of locator data.)
     */
    private String publicId;

    /**
     * The SAX system id for the resource being tokenized. (Only passed to back
     * as part of locator data.)
     */
    private String systemId;

    /**
     * Buffer for short identifiers.
     */
    private char[] strBuf;

    /**
     * Number of significant <code>char</code>s in <code>strBuf</code>.
     */
    private int strBufLen;

    /**
     * <code>-1</code> to indicate that <code>strBuf</code> is used or otherwise
     * an offset to the main buffer.
     */
    // private int strBufOffset = -1;
    /**
     * Buffer for long strings.
     */
    private char[] longStrBuf;

    /**
     * Number of significant <code>char</code>s in <code>longStrBuf</code>.
     */
    private int longStrBufLen;

    /**
     * <code>-1</code> to indicate that <code>longStrBuf</code> is used or
     * otherwise an offset to the main buffer.
     */
    // private int longStrBufOffset = -1;

    /**
     * Buffer for expanding NCRs falling into the Basic Multilingual Plane.
     */
    private final char[] bmpChar;

    /**
     * Buffer for expanding astral NCRs.
     */
    private final char[] astralChar;

    /**
     * The element whose end tag closes the current CDATA or RCDATA element.
     */
    protected ElementName endTagExpectation = null;

    private char[] endTagExpectationAsArray;

    /**
     * <code>true</code> if tokenizing an end tag
     */
    protected boolean endTag;

    /**
     * The current tag token name.
     */
    private ElementName tagName = null;

    /**
     * The current attribute name.
     */
    protected AttributeName attributeName = null;

    // [NOCPP[

    /**
     * Whether comment tokens are emitted.
     */
    private boolean wantsComments = false;

    /**
     * <code>true</code> when HTML4-specific additional errors are requested.
     */
    protected boolean html4;

    /**
     * Whether the stream is past the first 512 bytes.
     */
    private boolean metaBoundaryPassed;

    // ]NOCPP]

    /**
     * The name of the current doctype token.
     */
    private @Local String doctypeName;

    /**
     * The public id of the current doctype token.
     */
    private String publicIdentifier;

    /**
     * The system id of the current doctype token.
     */
    private String systemIdentifier;

    /**
     * The attribute holder.
     */
    private HtmlAttributes attributes;

    // [NOCPP[

    /**
     * The policy for vertical tab and form feed.
     */
    private XmlViolationPolicy contentSpacePolicy = XmlViolationPolicy.ALTER_INFOSET;

    /**
     * The policy for comments.
     */
    private XmlViolationPolicy commentPolicy = XmlViolationPolicy.ALTER_INFOSET;

    private XmlViolationPolicy xmlnsPolicy = XmlViolationPolicy.ALTER_INFOSET;

    private XmlViolationPolicy namePolicy = XmlViolationPolicy.ALTER_INFOSET;

    private boolean html4ModeCompatibleWithXhtml1Schemata;

    private final boolean newAttributesEachTime;

    // ]NOCPP]

    private int mappingLangToXmlLang;

    private boolean shouldSuspend;

    protected boolean confident;

    private int line;

    private Interner interner;

    // [NOCPP[

    protected LocatorImpl ampersandLocation;

    public Tokenizer(TokenHandler tokenHandler, boolean newAttributesEachTime) {
        this.tokenHandler = tokenHandler;
        this.encodingDeclarationHandler = null;
        this.newAttributesEachTime = newAttributesEachTime;
        this.bmpChar = new char[1];
        this.astralChar = new char[2];
        this.tagName = null;
        this.attributeName = null;
        this.doctypeName = null;
        this.publicIdentifier = null;
        this.systemIdentifier = null;
        this.attributes = null;
    }

    // ]NOCPP]

    /**
     * The constructor.
     * 
     * @param tokenHandler
     *            the handler for receiving tokens
     */
    public Tokenizer(TokenHandler tokenHandler) {
        this.tokenHandler = tokenHandler;
        this.encodingDeclarationHandler = null;
        // [NOCPP[
        this.newAttributesEachTime = false;
        // ]NOCPP]
        this.bmpChar = new char[1];
        this.astralChar = new char[2];
        this.tagName = null;
        this.attributeName = null;
        this.doctypeName = null;
        this.publicIdentifier = null;
        this.systemIdentifier = null;
        this.attributes = null;
    }

    public void setInterner(Interner interner) {
        this.interner = interner;
    }

    public void initLocation(String newPublicId, String newSystemId) {
        this.systemId = newSystemId;
        this.publicId = newPublicId;

    }

    void destructor() {
        Portability.releaseArray(bmpChar);
        Portability.releaseArray(astralChar);
    }

    // [NOCPP[

    /**
     * Returns the mappingLangToXmlLang.
     * 
     * @return the mappingLangToXmlLang
     */
    public boolean isMappingLangToXmlLang() {
        return mappingLangToXmlLang == AttributeName.HTML_LANG;
    }

    /**
     * Sets the mappingLangToXmlLang.
     * 
     * @param mappingLangToXmlLang
     *            the mappingLangToXmlLang to set
     */
    public void setMappingLangToXmlLang(boolean mappingLangToXmlLang) {
        this.mappingLangToXmlLang = mappingLangToXmlLang ? AttributeName.HTML_LANG
                : AttributeName.HTML;
    }

    /**
     * Sets the error handler.
     * 
     * @see org.xml.sax.XMLReader#setErrorHandler(org.xml.sax.ErrorHandler)
     */
    public void setErrorHandler(ErrorHandler eh) {
        this.errorHandler = eh;
    }

    public ErrorHandler getErrorHandler() {
        return this.errorHandler;
    }

    /**
     * Sets the commentPolicy.
     * 
     * @param commentPolicy
     *            the commentPolicy to set
     */
    public void setCommentPolicy(XmlViolationPolicy commentPolicy) {
        this.commentPolicy = commentPolicy;
    }

    /**
     * Sets the contentNonXmlCharPolicy.
     * 
     * @param contentNonXmlCharPolicy
     *            the contentNonXmlCharPolicy to set
     */
    public void setContentNonXmlCharPolicy(
            XmlViolationPolicy contentNonXmlCharPolicy) {
        if (contentNonXmlCharPolicy != XmlViolationPolicy.ALLOW) {
            throw new IllegalArgumentException(
                    "Must use ErrorReportingTokenizer to set contentNonXmlCharPolicy to non-ALLOW.");
        }
    }

    /**
     * Sets the contentSpacePolicy.
     * 
     * @param contentSpacePolicy
     *            the contentSpacePolicy to set
     */
    public void setContentSpacePolicy(XmlViolationPolicy contentSpacePolicy) {
        this.contentSpacePolicy = contentSpacePolicy;
    }

    /**
     * Sets the xmlnsPolicy.
     * 
     * @param xmlnsPolicy
     *            the xmlnsPolicy to set
     */
    public void setXmlnsPolicy(XmlViolationPolicy xmlnsPolicy) {
        if (xmlnsPolicy == XmlViolationPolicy.FATAL) {
            throw new IllegalArgumentException("Can't use FATAL here.");
        }
        this.xmlnsPolicy = xmlnsPolicy;
    }

    public void setNamePolicy(XmlViolationPolicy namePolicy) {
        this.namePolicy = namePolicy;
    }

    /**
     * Sets the html4ModeCompatibleWithXhtml1Schemata.
     * 
     * @param html4ModeCompatibleWithXhtml1Schemata
     *            the html4ModeCompatibleWithXhtml1Schemata to set
     */
    public void setHtml4ModeCompatibleWithXhtml1Schemata(
            boolean html4ModeCompatibleWithXhtml1Schemata) {
        this.html4ModeCompatibleWithXhtml1Schemata = html4ModeCompatibleWithXhtml1Schemata;
    }

    // ]NOCPP]

    // For the token handler to call
    /**
     * Sets the tokenizer state and the associated element name. This should 
     * only ever used to put the tokenizer into one of the states that have
     * a special end tag expectation.
     * 
     * @param specialTokenizerState
     *            the tokenizer state to set
     * @param endTagExpectation
     *            the expected end tag for transitioning back to normal
     */
    public void setStateAndEndTagExpectation(int specialTokenizerState,
            @Local String endTagExpectation) {
        this.stateSave = specialTokenizerState;
        if (specialTokenizerState == Tokenizer.DATA) {
            return;
        }
        char[] asArray = Portability.newCharArrayFromLocal(endTagExpectation);
        this.endTagExpectation = ElementName.elementNameByBuffer(asArray, 0,
                asArray.length, interner);
        Portability.releaseArray(asArray);
        endTagExpectationToArray();
    }

    /**
     * Sets the tokenizer state and the associated element name. This should 
     * only ever used to put the tokenizer into one of the states that have
     * a special end tag expectation.
     * 
     * @param specialTokenizerState
     *            the tokenizer state to set
     * @param endTagExpectation
     *            the expected end tag for transitioning back to normal
     */
    public void setStateAndEndTagExpectation(int specialTokenizerState,
            ElementName endTagExpectation) {
        this.stateSave = specialTokenizerState;
        this.endTagExpectation = endTagExpectation;
        endTagExpectationToArray();
    }

    private void endTagExpectationToArray() {
        switch (endTagExpectation.group) {
            case TreeBuilder.TITLE:
                endTagExpectationAsArray = TITLE_ARR;
                return;
            case TreeBuilder.SCRIPT:
                endTagExpectationAsArray = SCRIPT_ARR;
                return;
            case TreeBuilder.STYLE:
                endTagExpectationAsArray = STYLE_ARR;
                return;
            case TreeBuilder.PLAINTEXT:
                endTagExpectationAsArray = PLAINTEXT_ARR;
                return;
            case TreeBuilder.XMP:
                endTagExpectationAsArray = XMP_ARR;
                return;
            case TreeBuilder.TEXTAREA:
                endTagExpectationAsArray = TEXTAREA_ARR;
                return;
            case TreeBuilder.IFRAME:
                endTagExpectationAsArray = IFRAME_ARR;
                return;
            case TreeBuilder.NOEMBED:
                endTagExpectationAsArray = NOEMBED_ARR;
                return;
            case TreeBuilder.NOSCRIPT:
                endTagExpectationAsArray = NOSCRIPT_ARR;
                return;
            case TreeBuilder.NOFRAMES:
                endTagExpectationAsArray = NOFRAMES_ARR;
                return;
            default:
                assert false: "Bad end tag expectation.";
                return;
        }
    }

    /**
     * For C++ use only.
     */
    public void setLineNumber(int line) {
        this.line = line;
    }

    // start Locator impl

    /**
     * @see org.xml.sax.Locator#getLineNumber()
     */
    @Inline public int getLineNumber() {
        return line;
    }

    // [NOCPP[

    /**
     * @see org.xml.sax.Locator#getColumnNumber()
     */
    @Inline public int getColumnNumber() {
        return -1;
    }

    /**
     * @see org.xml.sax.Locator#getPublicId()
     */
    public String getPublicId() {
        return publicId;
    }

    /**
     * @see org.xml.sax.Locator#getSystemId()
     */
    public String getSystemId() {
        return systemId;
    }

    // end Locator impl

    // end public API

    public void notifyAboutMetaBoundary() {
        metaBoundaryPassed = true;
    }

    void turnOnAdditionalHtml4Errors() {
        html4 = true;
    }

    // ]NOCPP]

    HtmlAttributes emptyAttributes() {
        // [NOCPP[
        if (newAttributesEachTime) {
            return new HtmlAttributes(mappingLangToXmlLang);
        } else {
            // ]NOCPP]
            return HtmlAttributes.EMPTY_ATTRIBUTES;
            // [NOCPP[
        }
        // ]NOCPP]
    }

    @Inline private void clearStrBufAndAppend(char c) {
        strBuf[0] = c;
        strBufLen = 1;
    }

    @Inline private void clearStrBuf() {
        strBufLen = 0;
    }

    /**
     * Appends to the smaller buffer.
     * 
     * @param c
     *            the UTF-16 code unit to append
     */
    private void appendStrBuf(char c) {
        if (strBufLen == strBuf.length) {
            char[] newBuf = new char[strBuf.length + Tokenizer.BUFFER_GROW_BY];
            System.arraycopy(strBuf, 0, newBuf, 0, strBuf.length);
            Portability.releaseArray(strBuf);
            strBuf = newBuf;
        }
        strBuf[strBufLen++] = c;
    }

    /**
     * The smaller buffer as a String. Currently only used for error reporting.
     * 
     * <p>
     * C++ memory note: The return value must be released.
     * 
     * @return the smaller buffer as a string
     */
    protected String strBufToString() {
        return Portability.newStringFromBuffer(strBuf, 0, strBufLen);
    }

    /**
     * Returns the short buffer as a local name. The return value is released in
     * emitDoctypeToken().
     * 
     * @return the smaller buffer as local name
     */
    private void strBufToDoctypeName() {
        doctypeName = Portability.newLocalNameFromBuffer(strBuf, 0, strBufLen,
                interner);
    }

    /**
     * Emits the smaller buffer as character tokens.
     * 
     * @throws SAXException
     *             if the token handler threw
     */
    private void emitStrBuf() throws SAXException {
        if (strBufLen > 0) {
            tokenHandler.characters(strBuf, 0, strBufLen);
        }
    }

    @Inline private void clearLongStrBuf() {
        longStrBufLen = 0;
    }

    @Inline private void clearLongStrBufAndAppend(char c) {
        longStrBuf[0] = c;
        longStrBufLen = 1;
    }

    /**
     * Appends to the larger buffer.
     * 
     * @param c
     *            the UTF-16 code unit to append
     */
    private void appendLongStrBuf(char c) {
        if (longStrBufLen == longStrBuf.length) {
            char[] newBuf = new char[longStrBufLen + (longStrBufLen >> 1)];
            System.arraycopy(longStrBuf, 0, newBuf, 0, longStrBuf.length);
            Portability.releaseArray(longStrBuf);
            longStrBuf = newBuf;
        }
        longStrBuf[longStrBufLen++] = c;
    }

    @Inline private void appendSecondHyphenToBogusComment() throws SAXException {
        // [NOCPP[
        switch (commentPolicy) {
            case ALTER_INFOSET:
                // detachLongStrBuf();
                appendLongStrBuf(' ');
                // FALLTHROUGH
            case ALLOW:
                warn("The document is not mappable to XML 1.0 due to two consecutive hyphens in a comment.");
                // ]NOCPP]
                appendLongStrBuf('-');
                // [NOCPP[
                break;
            case FATAL:
                fatal("The document is not mappable to XML 1.0 due to two consecutive hyphens in a comment.");
                break;
        }
        // ]NOCPP]
    }

    // [NOCPP[
    private void maybeAppendSpaceToBogusComment() throws SAXException {
        switch (commentPolicy) {
            case ALTER_INFOSET:
                // detachLongStrBuf();
                appendLongStrBuf(' ');
                // FALLTHROUGH
            case ALLOW:
                warn("The document is not mappable to XML 1.0 due to a trailing hyphen in a comment.");
                break;
            case FATAL:
                fatal("The document is not mappable to XML 1.0 due to a trailing hyphen in a comment.");
                break;
        }
    }

    // ]NOCPP]

    @Inline private void adjustDoubleHyphenAndAppendToLongStrBufAndErr(char c)
            throws SAXException {
        errConsecutiveHyphens();
        // [NOCPP[
        switch (commentPolicy) {
            case ALTER_INFOSET:
                // detachLongStrBuf();
                longStrBufLen--;
                appendLongStrBuf(' ');
                appendLongStrBuf('-');
                // FALLTHROUGH
            case ALLOW:
                warn("The document is not mappable to XML 1.0 due to two consecutive hyphens in a comment.");
                // ]NOCPP]
                appendLongStrBuf(c);
                // [NOCPP[
                break;
            case FATAL:
                fatal("The document is not mappable to XML 1.0 due to two consecutive hyphens in a comment.");
                break;
        }
        // ]NOCPP]
    }

    private void appendLongStrBuf(char[] buffer, int offset, int length) {
        int reqLen = longStrBufLen + length;
        if (longStrBuf.length < reqLen) {
            char[] newBuf = new char[reqLen + (reqLen >> 1)];
            System.arraycopy(longStrBuf, 0, newBuf, 0, longStrBuf.length);
            Portability.releaseArray(longStrBuf);
            longStrBuf = newBuf;
        }
        System.arraycopy(buffer, offset, longStrBuf, longStrBufLen, length);
        longStrBufLen = reqLen;
    }

    /**
     * Append the contents of the smaller buffer to the larger one.
     */
    @Inline private void appendStrBufToLongStrBuf() {
        appendLongStrBuf(strBuf, 0, strBufLen);
    }

    /**
     * The larger buffer as a string.
     * 
     * <p>
     * C++ memory note: The return value must be released.
     * 
     * @return the larger buffer as a string
     */
    private String longStrBufToString() {
        return Portability.newStringFromBuffer(longStrBuf, 0, longStrBufLen);
    }

    /**
     * Emits the current comment token.
     * 
     * @param pos
     *            TODO
     * 
     * @throws SAXException
     */
    private void emitComment(int provisionalHyphens, int pos)
            throws SAXException {
        // [NOCPP[
        if (wantsComments) {
            // ]NOCPP]
            // if (longStrBufOffset != -1) {
            // tokenHandler.comment(buf, longStrBufOffset, longStrBufLen
            // - provisionalHyphens);
            // } else {
            tokenHandler.comment(longStrBuf, 0, longStrBufLen
                    - provisionalHyphens);
            // }
            // [NOCPP[
        }
        // ]NOCPP]
        cstart = pos + 1;
    }

    /**
     * Flushes coalesced character tokens.
     * 
     * @param buf
     *            TODO
     * @param pos
     *            TODO
     * 
     * @throws SAXException
     */
    protected void flushChars(@NoLength char[] buf, int pos)
            throws SAXException {
        if (pos > cstart) {
            tokenHandler.characters(buf, cstart, pos - cstart);
        }
        cstart = Integer.MAX_VALUE;
    }

    /**
     * Reports an condition that would make the infoset incompatible with XML
     * 1.0 as fatal.
     * 
     * @param message
     *            the message
     * @throws SAXException
     * @throws SAXParseException
     */
    public void fatal(String message) throws SAXException {
        SAXParseException spe = new SAXParseException(message, this);
        if (errorHandler != null) {
            errorHandler.fatalError(spe);
        }
        throw spe;
    }

    /**
     * Reports a Parse Error.
     * 
     * @param message
     *            the message
     * @throws SAXException
     */
    public void err(String message) throws SAXException {
        if (errorHandler == null) {
            return;
        }
        SAXParseException spe = new SAXParseException(message, this);
        errorHandler.error(spe);
    }

    public void errTreeBuilder(String message) throws SAXException {
        ErrorHandler eh = null;
        if (tokenHandler instanceof TreeBuilder<?>) {
            TreeBuilder<?> treeBuilder = (TreeBuilder<?>) tokenHandler;
            eh = treeBuilder.getErrorHandler();
        }
        if (eh == null) {
            eh = errorHandler;
        }
        if (eh == null) {
            return;
        }
        SAXParseException spe = new SAXParseException(message, this);
        eh.error(spe);
    }

    /**
     * Reports a warning
     * 
     * @param message
     *            the message
     * @throws SAXException
     */
    public void warn(String message) throws SAXException {
        if (errorHandler == null) {
            return;
        }
        SAXParseException spe = new SAXParseException(message, this);
        errorHandler.warning(spe);
    }

    /**
     * 
     */
    private void resetAttributes() {
        // [NOCPP[
        if (newAttributesEachTime) {
            // ]NOCPP]
            attributes = null;
            // [NOCPP[
        } else {
            attributes.clear(mappingLangToXmlLang);
        }
        // ]NOCPP]
    }

    private void strBufToElementNameString() {
        // if (strBufOffset != -1) {
        // return ElementName.elementNameByBuffer(buf, strBufOffset, strBufLen);
        // } else {
        tagName = ElementName.elementNameByBuffer(strBuf, 0, strBufLen,
                interner);
        // }
    }

    private int emitCurrentTagToken(boolean selfClosing, int pos)
            throws SAXException {
        cstart = pos + 1;
        maybeErrSlashInEndTag(selfClosing);
        stateSave = Tokenizer.DATA;
        HtmlAttributes attrs = (attributes == null ? HtmlAttributes.EMPTY_ATTRIBUTES
                : attributes);
        if (endTag) {
            /*
             * When an end tag token is emitted, the content model flag must be
             * switched to the PCDATA state.
             */
            maybeErrAttributesOnEndTag(attrs);
            tokenHandler.endTag(tagName);
            Portability.delete(attributes);
        } else {
            tokenHandler.startTag(tagName, attrs, selfClosing);
        }
        tagName.release();
        tagName = null;
        resetAttributes();
        /*
         * The token handler may have called setStateAndEndTagExpectation
         * and changed stateSave since the start of this method.
         */
        return stateSave;
    }

    private void attributeNameComplete() throws SAXException {
        // if (strBufOffset != -1) {
        // attributeName = AttributeName.nameByBuffer(buf, strBufOffset,
        // strBufLen, namePolicy != XmlViolationPolicy.ALLOW);
        // } else {
        attributeName = AttributeName.nameByBuffer(strBuf, 0, strBufLen
        // [NOCPP[
                , namePolicy != XmlViolationPolicy.ALLOW
                // ]NOCPP]
                , interner);
        // }

        if (attributes == null) {
            attributes = new HtmlAttributes(mappingLangToXmlLang);
        }

        /*
         * When the user agent leaves the attribute name state (and before
         * emitting the tag token, if appropriate), the complete attribute's
         * name must be compared to the other attributes on the same token; if
         * there is already an attribute on the token with the exact same name,
         * then this is a parse error and the new attribute must be dropped,
         * along with the value that gets associated with it (if any).
         */
        if (attributes.contains(attributeName)) {
            errDuplicateAttribute();
            attributeName.release();
            attributeName = null;
        }
    }

    private void addAttributeWithoutValue() throws SAXException {
        noteAttributeWithoutValue();

        // [NOCPP[
        if (metaBoundaryPassed && AttributeName.CHARSET == attributeName
                && ElementName.META == tagName) {
            err("A \u201Ccharset\u201D attribute on a \u201Cmeta\u201D element found after the first 512 bytes.");
        }
        // ]NOCPP]
        if (attributeName != null) {
            // [NOCPP[
            if (html4) {
                if (attributeName.isBoolean()) {
                    if (html4ModeCompatibleWithXhtml1Schemata) {
                        attributes.addAttribute(attributeName,
                                attributeName.getLocal(AttributeName.HTML),
                                xmlnsPolicy);
                    } else {
                        attributes.addAttribute(attributeName, "", xmlnsPolicy);
                    }
                } else {
                    err("Attribute value omitted for a non-boolean attribute. (HTML4-only error.)");
                    attributes.addAttribute(attributeName, "", xmlnsPolicy);
                }
            } else {
                if (AttributeName.SRC == attributeName
                        || AttributeName.HREF == attributeName) {
                    warn("Attribute \u201C"
                            + attributeName.getLocal(AttributeName.HTML)
                            + "\u201D without an explicit value seen. The attribute may be dropped by IE7.");
                }
                // ]NOCPP]
                attributes.addAttribute(attributeName,
                        Portability.newEmptyString()
                        // [NOCPP[
                        , xmlnsPolicy
                // ]NOCPP]
                );
                // [NOCPP[
            }
            // ]NOCPP]
            attributeName = null; // attributeName has been adopted by the
            // |attributes| object
        }
    }

    private void addAttributeWithValue() throws SAXException {
        // [NOCPP[
        if (metaBoundaryPassed && ElementName.META == tagName
                && AttributeName.CHARSET == attributeName) {
            err("A \u201Ccharset\u201D attribute on a \u201Cmeta\u201D element found after the first 512 bytes.");
        }
        // ]NOCPP]
        if (attributeName != null) {
            String val = longStrBufToString(); // Ownership transferred to
            // HtmlAttributes
            // [NOCPP[
            if (!endTag && html4 && html4ModeCompatibleWithXhtml1Schemata
                    && attributeName.isCaseFolded()) {
                val = newAsciiLowerCaseStringFromString(val);
            }
            // ]NOCPP]
            attributes.addAttribute(attributeName, val
            // [NOCPP[
                    , xmlnsPolicy
            // ]NOCPP]
            );
            attributeName = null; // attributeName has been adopted by the
            // |attributes| object
        }
    }

    // [NOCPP[

    private static String newAsciiLowerCaseStringFromString(String str) {
        if (str == null) {
            return null;
        }
        char[] buf = new char[str.length()];
        for (int i = 0; i < str.length(); i++) {
            char c = str.charAt(i);
            if (c >= 'A' && c <= 'Z') {
                c += 0x20;
            }
            buf[i] = c;
        }
        return new String(buf);
    }

    protected void startErrorReporting() throws SAXException {

    }

    // ]NOCPP]
    
    public void start() throws SAXException {
        initializeWithoutStarting();
        tokenHandler.startTokenization(this);
        // [NOCPP[
        startErrorReporting();
        // ]NOCPP]
    }

    public boolean tokenizeBuffer(UTF16Buffer buffer) throws SAXException {
        int state = stateSave;
        int returnState = returnStateSave;
        char c = '\u0000';
        shouldSuspend = false;
        lastCR = false;

        int start = buffer.getStart();
        /**
         * The index of the last <code>char</code> read from <code>buf</code>.
         */
        int pos = start - 1;

        /**
         * The index of the first <code>char</code> in <code>buf</code> that is
         * part of a coalesced run of character tokens or
         * <code>Integer.MAX_VALUE</code> if there is not a current run being
         * coalesced.
         */
        switch (state) {
            case DATA:
            case RCDATA:
            case SCRIPT_DATA:
            case PLAINTEXT:
            case RAWTEXT:
            case CDATA_SECTION:
            case SCRIPT_DATA_ESCAPED:
            case SCRIPT_DATA_ESCAPE_START:
            case SCRIPT_DATA_ESCAPE_START_DASH:
            case SCRIPT_DATA_ESCAPED_DASH:
            case SCRIPT_DATA_ESCAPED_DASH_DASH:
            case SCRIPT_DATA_DOUBLE_ESCAPE_START:
            case SCRIPT_DATA_DOUBLE_ESCAPED:
            case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN:
            case SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
            case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
            case SCRIPT_DATA_DOUBLE_ESCAPE_END:
                cstart = start;
                break;
            default:
                cstart = Integer.MAX_VALUE;
                break;
        }

        /**
         * The number of <code>char</code>s in <code>buf</code> that have
         * meaning. (The rest of the array is garbage and should not be
         * examined.)
         */
        pos = stateLoop(state, c, pos, buffer.getBuffer(), false, returnState,
                buffer.getEnd());
        if (pos == buffer.getEnd()) {
            // exiting due to end of buffer
            buffer.setStart(pos);
        } else {
            buffer.setStart(pos + 1);
        }
        return lastCR;
    }

    @SuppressWarnings("unused") private int stateLoop(int state, char c,
            int pos, @NoLength char[] buf, boolean reconsume, int returnState,
            int endPos) throws SAXException {
        /*
         * Idioms used in this code:
         * 
         * 
         * Consuming the next input character
         * 
         * To consume the next input character, the code does this: if (++pos ==
         * endPos) { break stateloop; } c = checkChar(buf, pos);
         * 
         * 
         * Staying in a state
         * 
         * When there's a state that the tokenizer may stay in over multiple
         * input characters, the state has a wrapper |for(;;)| loop and staying
         * in the state continues the loop.
         * 
         * 
         * Switching to another state
         * 
         * To switch to another state, the code sets the state variable to the
         * magic number of the new state. Then it either continues stateloop or
         * breaks out of the state's own wrapper loop if the target state is
         * right after the current state in source order. (This is a partial
         * workaround for Java's lack of goto.)
         * 
         * 
         * Reconsume support
         * 
         * The spec sometimes says that an input character is reconsumed in
         * another state. If a state can ever be entered so that an input
         * character can be reconsumed in it, the state's code starts with an
         * |if (reconsume)| that sets reconsume to false and skips over the
         * normal code for consuming a new character.
         * 
         * To reconsume the current character in another state, the code sets
         * |reconsume| to true and then switches to the other state.
         * 
         * 
         * Emitting character tokens
         * 
         * This method emits character tokens lazily. Whenever a new range of
         * character tokens starts, the field cstart must be set to the start
         * index of the range. The flushChars() method must be called at the end
         * of a range to flush it.
         * 
         * 
         * U+0000 handling
         * 
         * The various states have to handle the replacement of U+0000 with
         * U+FFFD. However, if U+0000 would be reconsumed in another state, the
         * replacement doesn't need to happen, because it's handled by the
         * reconsuming state.
         * 
         * 
         * LF handling
         * 
         * Every state needs to increment the line number upon LF unless the LF
         * gets reconsumed by another state which increments the line number.
         * 
         * 
         * CR handling
         * 
         * Every state needs to handle CR unless the CR gets reconsumed and is
         * handled by the reconsuming state. The CR needs to be handled as if it
         * were and LF, the lastCR field must be set to true and then this
         * method must return. The IO driver will then swallow the next
         * character if it is an LF to coalesce CRLF.
         */
        stateloop: for (;;) {
            switch (state) {
                case DATA:
                    dataloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        switch (c) {
                            case '&':
                                /*
                                 * U+0026 AMPERSAND (&) Switch to the character
                                 * reference in data state.
                                 */
                                flushChars(buf, pos);
                                clearStrBufAndAppend(c);
                                setAdditionalAndRememberAmpersandLocation('\u0000');
                                returnState = state;
                                state = Tokenizer.CONSUME_CHARACTER_REFERENCE;
                                continue stateloop;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the tag
                                 * open state.
                                 */
                                flushChars(buf, pos);

                                state = Tokenizer.TAG_OPEN;
                                break dataloop; // FALL THROUGH continue
                            // stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the input character as a
                                 * character token.
                                 * 
                                 * Stay in the data state.
                                 */
                                continue;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case TAG_OPEN:
                    tagopenloop: for (;;) {
                        /*
                         * The behavior of this state depends on the content
                         * model flag.
                         */
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * If the content model flag is set to the PCDATA state
                         * Consume the next input character:
                         */
                        if (c >= 'A' && c <= 'Z') {
                            /*
                             * U+0041 LATIN CAPITAL LETTER A through to U+005A
                             * LATIN CAPITAL LETTER Z Create a new start tag
                             * token,
                             */
                            endTag = false;
                            /*
                             * set its tag name to the lowercase version of the
                             * input character (add 0x0020 to the character's
                             * code point),
                             */
                            clearStrBufAndAppend((char) (c + 0x20));
                            /* then switch to the tag name state. */
                            state = Tokenizer.TAG_NAME;
                            /*
                             * (Don't emit the token yet; further details will
                             * be filled in before it is emitted.)
                             */
                            break tagopenloop;
                            // continue stateloop;
                        } else if (c >= 'a' && c <= 'z') {
                            /*
                             * U+0061 LATIN SMALL LETTER A through to U+007A
                             * LATIN SMALL LETTER Z Create a new start tag
                             * token,
                             */
                            endTag = false;
                            /*
                             * set its tag name to the input character,
                             */
                            clearStrBufAndAppend(c);
                            /* then switch to the tag name state. */
                            state = Tokenizer.TAG_NAME;
                            /*
                             * (Don't emit the token yet; further details will
                             * be filled in before it is emitted.)
                             */
                            break tagopenloop;
                            // continue stateloop;
                        }
                        switch (c) {
                            case '!':
                                /*
                                 * U+0021 EXCLAMATION MARK (!) Switch to the
                                 * markup declaration open state.
                                 */
                                state = Tokenizer.MARKUP_DECLARATION_OPEN;
                                continue stateloop;
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Switch to the close tag
                                 * open state.
                                 */
                                state = Tokenizer.CLOSE_TAG_OPEN;
                                continue stateloop;
                            case '?':
                                /*
                                 * U+003F QUESTION MARK (?) Parse error.
                                 */
                                errProcessingInstruction();
                                /*
                                 * Switch to the bogus comment state.
                                 */
                                clearLongStrBufAndAppend(c);
                                state = Tokenizer.BOGUS_COMMENT;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Parse error.
                                 */
                                errLtGt();
                                /*
                                 * Emit a U+003C LESS-THAN SIGN character token
                                 * and a U+003E GREATER-THAN SIGN character
                                 * token.
                                 */
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 2);
                                /* Switch to the data state. */
                                cstart = pos + 1;
                                state = Tokenizer.DATA;
                                continue stateloop;
                            default:
                                /*
                                 * Anything else Parse error.
                                 */
                                errBadCharAfterLt(c);
                                /*
                                 * Emit a U+003C LESS-THAN SIGN character token
                                 */
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                                /*
                                 * and reconsume the current input character in
                                 * the data state.
                                 */
                                cstart = pos;
                                state = Tokenizer.DATA;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // FALL THROUGH DON'T REORDER
                case TAG_NAME:
                    tagnameloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                strBufToElementNameString();
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the before attribute name state.
                                 */
                                strBufToElementNameString();
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                break tagnameloop;
                            // continue stateloop;
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Switch to the self-closing
                                 * start tag state.
                                 */
                                strBufToElementNameString();
                                state = Tokenizer.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * tag token.
                                 */
                                strBufToElementNameString();
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                if (c >= 'A' && c <= 'Z') {
                                    /*
                                     * U+0041 LATIN CAPITAL LETTER A through to
                                     * U+005A LATIN CAPITAL LETTER Z Append the
                                     * lowercase version of the current input
                                     * character (add 0x0020 to the character's
                                     * code point) to the current tag token's
                                     * tag name.
                                     */
                                    c += 0x20;
                                }
                                /*
                                 * Anything else Append the current input
                                 * character to the current tag token's tag
                                 * name.
                                 */
                                appendStrBuf(c);
                                /*
                                 * Stay in the tag name state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BEFORE_ATTRIBUTE_NAME:
                    beforeattributenameloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the before attribute name state.
                                 */
                                continue;
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Switch to the self-closing
                                 * start tag state.
                                 */
                                state = Tokenizer.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * tag token.
                                 */
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            case '\"':
                            case '\'':
                            case '<':
                            case '=':
                                /*
                                 * U+0022 QUOTATION MARK (") U+0027 APOSTROPHE
                                 * (') U+003C LESS-THAN SIGN (<) U+003D EQUALS
                                 * SIGN (=) Parse error.
                                 */
                                errBadCharBeforeAttributeNameOrNull(c);
                                /*
                                 * Treat it as per the "anything else" entry
                                 * below.
                                 */
                            default:
                                /*
                                 * Anything else Start a new attribute in the
                                 * current tag token.
                                 */
                                if (c >= 'A' && c <= 'Z') {
                                    /*
                                     * U+0041 LATIN CAPITAL LETTER A through to
                                     * U+005A LATIN CAPITAL LETTER Z Set that
                                     * attribute's name to the lowercase version
                                     * of the current input character (add
                                     * 0x0020 to the character's code point)
                                     */
                                    c += 0x20;
                                }
                                /*
                                 * Set that attribute's name to the current
                                 * input character,
                                 */
                                clearStrBufAndAppend(c);
                                /*
                                 * and its value to the empty string.
                                 */
                                // Will do later.
                                /*
                                 * Switch to the attribute name state.
                                 */
                                state = Tokenizer.ATTRIBUTE_NAME;
                                break beforeattributenameloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case ATTRIBUTE_NAME:
                    attributenameloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                attributeNameComplete();
                                state = Tokenizer.AFTER_ATTRIBUTE_NAME;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the after attribute name state.
                                 */
                                attributeNameComplete();
                                state = Tokenizer.AFTER_ATTRIBUTE_NAME;
                                continue stateloop;
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Switch to the self-closing
                                 * start tag state.
                                 */
                                attributeNameComplete();
                                addAttributeWithoutValue();
                                state = Tokenizer.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '=':
                                /*
                                 * U+003D EQUALS SIGN (=) Switch to the before
                                 * attribute value state.
                                 */
                                attributeNameComplete();
                                state = Tokenizer.BEFORE_ATTRIBUTE_VALUE;
                                break attributenameloop;
                            // continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * tag token.
                                 */
                                attributeNameComplete();
                                addAttributeWithoutValue();
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            case '\"':
                            case '\'':
                            case '<':
                                /*
                                 * U+0022 QUOTATION MARK (") U+0027 APOSTROPHE
                                 * (') U+003C LESS-THAN SIGN (<) Parse error.
                                 */
                                errQuoteOrLtInAttributeNameOrNull(c);
                                /*
                                 * Treat it as per the "anything else" entry
                                 * below.
                                 */
                            default:
                                if (c >= 'A' && c <= 'Z') {
                                    /*
                                     * U+0041 LATIN CAPITAL LETTER A through to
                                     * U+005A LATIN CAPITAL LETTER Z Append the
                                     * lowercase version of the current input
                                     * character (add 0x0020 to the character's
                                     * code point) to the current attribute's
                                     * name.
                                     */
                                    c += 0x20;
                                }
                                /*
                                 * Anything else Append the current input
                                 * character to the current attribute's name.
                                 */
                                appendStrBuf(c);
                                /*
                                 * Stay in the attribute name state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BEFORE_ATTRIBUTE_VALUE:
                    beforeattributevalueloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the before attribute value state.
                                 */
                                continue;
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Switch to the
                                 * attribute value (double-quoted) state.
                                 */
                                clearLongStrBuf();
                                state = Tokenizer.ATTRIBUTE_VALUE_DOUBLE_QUOTED;
                                break beforeattributevalueloop;
                            // continue stateloop;
                            case '&':
                                /*
                                 * U+0026 AMPERSAND (&) Switch to the attribute
                                 * value (unquoted) state and reconsume this
                                 * input character.
                                 */
                                clearLongStrBuf();
                                state = Tokenizer.ATTRIBUTE_VALUE_UNQUOTED;
                                noteUnquotedAttributeValue();
                                reconsume = true;
                                continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Switch to the attribute
                                 * value (single-quoted) state.
                                 */
                                clearLongStrBuf();
                                state = Tokenizer.ATTRIBUTE_VALUE_SINGLE_QUOTED;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Parse error.
                                 */
                                errAttributeValueMissing();
                                /*
                                 * Emit the current tag token.
                                 */
                                addAttributeWithoutValue();
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            case '<':
                            case '=':
                            case '`':
                                /*
                                 * U+003C LESS-THAN SIGN (<) U+003D EQUALS SIGN
                                 * (=) U+0060 GRAVE ACCENT (`)
                                 */
                                errLtOrEqualsOrGraveInUnquotedAttributeOrNull(c);
                                /*
                                 * Treat it as per the "anything else" entry
                                 * below.
                                 */
                            default:
                                // [NOCPP[
                                errHtml4NonNameInUnquotedAttribute(c);
                                // ]NOCPP]
                                /*
                                 * Anything else Append the current input
                                 * character to the current attribute's value.
                                 */
                                clearLongStrBufAndAppend(c);
                                /*
                                 * Switch to the attribute value (unquoted)
                                 * state.
                                 */

                                state = Tokenizer.ATTRIBUTE_VALUE_UNQUOTED;
                                noteUnquotedAttributeValue();
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case ATTRIBUTE_VALUE_DOUBLE_QUOTED:
                    attributevaluedoublequotedloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Switch to the after
                                 * attribute value (quoted) state.
                                 */
                                addAttributeWithValue();

                                state = Tokenizer.AFTER_ATTRIBUTE_VALUE_QUOTED;
                                break attributevaluedoublequotedloop;
                            // continue stateloop;
                            case '&':
                                /*
                                 * U+0026 AMPERSAND (&) Switch to the character
                                 * reference in attribute value state, with the
                                 * additional allowed character being U+0022
                                 * QUOTATION MARK (").
                                 */
                                clearStrBufAndAppend(c);
                                setAdditionalAndRememberAmpersandLocation('\"');
                                returnState = state;
                                state = Tokenizer.CONSUME_CHARACTER_REFERENCE;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the current input
                                 * character to the current attribute's value.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the attribute value (double-quoted)
                                 * state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case AFTER_ATTRIBUTE_VALUE_QUOTED:
                    afterattributevaluequotedloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the before attribute name state.
                                 */
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                continue stateloop;
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Switch to the self-closing
                                 * start tag state.
                                 */
                                state = Tokenizer.SELF_CLOSING_START_TAG;
                                break afterattributevaluequotedloop;
                            // continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * tag token.
                                 */
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            default:
                                /*
                                 * Anything else Parse error.
                                 */
                                errNoSpaceBetweenAttributes();
                                /*
                                 * Reconsume the character in the before
                                 * attribute name state.
                                 */
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case SELF_CLOSING_START_TAG:
                    if (++pos == endPos) {
                        break stateloop;
                    }
                    c = checkChar(buf, pos);
                    /*
                     * Consume the next input character:
                     */
                    switch (c) {
                        case '>':
                            /*
                             * U+003E GREATER-THAN SIGN (>) Set the self-closing
                             * flag of the current tag token. Emit the current
                             * tag token.
                             */
                            // [NOCPP[
                            errHtml4XmlVoidSyntax();
                            // ]NOCPP]
                            state = emitCurrentTagToken(true, pos);
                            if (shouldSuspend) {
                                break stateloop;
                            }
                            /*
                             * Switch to the data state.
                             */
                            continue stateloop;
                        default:
                            /* Anything else Parse error. */
                            errSlashNotFollowedByGt();
                            /*
                             * Reconsume the character in the before attribute
                             * name state.
                             */
                            state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                            reconsume = true;
                            continue stateloop;
                    }
                    // XXX reorder point
                case ATTRIBUTE_VALUE_UNQUOTED:
                    for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                addAttributeWithValue();
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the before attribute name state.
                                 */
                                addAttributeWithValue();
                                state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                continue stateloop;
                            case '&':
                                /*
                                 * U+0026 AMPERSAND (&) Switch to the character
                                 * reference in attribute value state, with the
                                 * additional allowed character being U+003E
                                 * GREATER-THAN SIGN (>)
                                 */
                                clearStrBufAndAppend(c);
                                setAdditionalAndRememberAmpersandLocation('>');
                                returnState = state;
                                state = Tokenizer.CONSUME_CHARACTER_REFERENCE;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * tag token.
                                 */
                                addAttributeWithValue();
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            case '<':
                            case '\"':
                            case '\'':
                            case '=':
                            case '`':
                                /*
                                 * U+0022 QUOTATION MARK (") U+0027 APOSTROPHE
                                 * (') U+003C LESS-THAN SIGN (<) U+003D EQUALS
                                 * SIGN (=) U+0060 GRAVE ACCENT (`) Parse error.
                                 */
                                errUnquotedAttributeValOrNull(c);
                                /*
                                 * Treat it as per the "anything else" entry
                                 * below.
                                 */
                                // fall through
                            default:
                                // [NOCPP]
                                errHtml4NonNameInUnquotedAttribute(c);
                                // ]NOCPP]
                                /*
                                 * Anything else Append the current input
                                 * character to the current attribute's value.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the attribute value (unquoted) state.
                                 */
                                continue;
                        }
                    }
                    // XXX reorder point
                case AFTER_ATTRIBUTE_NAME:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the after attribute name state.
                                 */
                                continue;
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Switch to the self-closing
                                 * start tag state.
                                 */
                                addAttributeWithoutValue();
                                state = Tokenizer.SELF_CLOSING_START_TAG;
                                continue stateloop;
                            case '=':
                                /*
                                 * U+003D EQUALS SIGN (=) Switch to the before
                                 * attribute value state.
                                 */
                                state = Tokenizer.BEFORE_ATTRIBUTE_VALUE;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * tag token.
                                 */
                                addAttributeWithoutValue();
                                state = emitCurrentTagToken(false, pos);
                                if (shouldSuspend) {
                                    break stateloop;
                                }
                                /*
                                 * Switch to the data state.
                                 */
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            case '\"':
                            case '\'':
                            case '<':
                                errQuoteOrLtInAttributeNameOrNull(c);
                                /*
                                 * Treat it as per the "anything else" entry
                                 * below.
                                 */
                            default:
                                addAttributeWithoutValue();
                                /*
                                 * Anything else Start a new attribute in the
                                 * current tag token.
                                 */
                                if (c >= 'A' && c <= 'Z') {
                                    /*
                                     * U+0041 LATIN CAPITAL LETTER A through to
                                     * U+005A LATIN CAPITAL LETTER Z Set that
                                     * attribute's name to the lowercase version
                                     * of the current input character (add
                                     * 0x0020 to the character's code point)
                                     */
                                    c += 0x20;
                                }
                                /*
                                 * Set that attribute's name to the current
                                 * input character,
                                 */
                                clearStrBufAndAppend(c);
                                /*
                                 * and its value to the empty string.
                                 */
                                // Will do later.
                                /*
                                 * Switch to the attribute name state.
                                 */
                                state = Tokenizer.ATTRIBUTE_NAME;
                                continue stateloop;
                        }
                    }
                    // XXX reorder point
                case MARKUP_DECLARATION_OPEN:
                    markupdeclarationopenloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * If the next two characters are both U+002D
                         * HYPHEN-MINUS characters (-), consume those two
                         * characters, create a comment token whose data is the
                         * empty string, and switch to the comment start state.
                         * 
                         * Otherwise, if the next seven characters are an ASCII
                         * case-insensitive match for the word "DOCTYPE", then
                         * consume those characters and switch to the DOCTYPE
                         * state.
                         * 
                         * Otherwise, if the insertion mode is
                         * "in foreign content" and the current node is not an
                         * element in the HTML namespace and the next seven
                         * characters are an case-sensitive match for the string
                         * "[CDATA[" (the five uppercase letters "CDATA" with a
                         * U+005B LEFT SQUARE BRACKET character before and
                         * after), then consume those characters and switch to
                         * the CDATA section state.
                         * 
                         * Otherwise, is is a parse error. Switch to the bogus
                         * comment state. The next character that is consumed,
                         * if any, is the first character that will be in the
                         * comment.
                         */
                        switch (c) {
                            case '-':
                                clearLongStrBufAndAppend(c);
                                state = Tokenizer.MARKUP_DECLARATION_HYPHEN;
                                break markupdeclarationopenloop;
                            // continue stateloop;
                            case 'd':
                            case 'D':
                                clearLongStrBufAndAppend(c);
                                index = 0;
                                state = Tokenizer.MARKUP_DECLARATION_OCTYPE;
                                continue stateloop;
                            case '[':
                                if (tokenHandler.cdataSectionAllowed()) {
                                    clearLongStrBufAndAppend(c);
                                    index = 0;
                                    state = Tokenizer.CDATA_START;
                                    continue stateloop;
                                }
                                // else fall through
                            default:
                                errBogusComment();
                                clearLongStrBuf();
                                state = Tokenizer.BOGUS_COMMENT;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case MARKUP_DECLARATION_HYPHEN:
                    markupdeclarationhyphenloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        switch (c) {
                            case '\u0000':
                                break stateloop;
                            case '-':
                                clearLongStrBuf();
                                state = Tokenizer.COMMENT_START;
                                break markupdeclarationhyphenloop;
                            // continue stateloop;
                            default:
                                errBogusComment();
                                state = Tokenizer.BOGUS_COMMENT;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case COMMENT_START:
                    commentstartloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Comment start state
                         * 
                         * 
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Switch to the comment
                                 * start dash state.
                                 */
                                appendLongStrBuf(c);
                                state = Tokenizer.COMMENT_START_DASH;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Parse error.
                                 */
                                errPrematureEndOfComment();
                                /* Emit the comment token. */
                                emitComment(0, pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                state = Tokenizer.COMMENT;
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                state = Tokenizer.COMMENT;
                                break commentstartloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the input character to
                                 * the comment token's data.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Switch to the comment state.
                                 */
                                state = Tokenizer.COMMENT;
                                break commentstartloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case COMMENT:
                    commentloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Comment state Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Switch to the comment
                                 * end dash state
                                 */
                                appendLongStrBuf(c);
                                state = Tokenizer.COMMENT_END_DASH;
                                break commentloop;
                            // continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the input character to
                                 * the comment token's data.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the comment state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case COMMENT_END_DASH:
                    commentenddashloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Comment end dash state Consume the next input
                         * character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Switch to the comment
                                 * end state
                                 */
                                appendLongStrBuf(c);
                                state = Tokenizer.COMMENT_END;
                                break commentenddashloop;
                            // continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                state = Tokenizer.COMMENT;
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                state = Tokenizer.COMMENT;
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append a U+002D HYPHEN-MINUS
                                 * (-) character and the input character to the
                                 * comment token's data.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Switch to the comment state.
                                 */
                                state = Tokenizer.COMMENT;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case COMMENT_END:
                    commentendloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Comment end dash state Consume the next input
                         * character:
                         */
                        switch (c) {
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the comment
                                 * token.
                                 */
                                emitComment(2, pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '-':
                                /* U+002D HYPHEN-MINUS (-) Parse error. */
                                /*
                                 * Append a U+002D HYPHEN-MINUS (-) character to
                                 * the comment token's data.
                                 */
                                adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
                                /*
                                 * Stay in the comment end state.
                                 */
                                continue;
                            case '\r':
                                adjustDoubleHyphenAndAppendToLongStrBufCarriageReturn();
                                state = Tokenizer.COMMENT;
                                break stateloop;
                            case '\n':
                                adjustDoubleHyphenAndAppendToLongStrBufLineFeed();
                                state = Tokenizer.COMMENT;
                                continue stateloop;
                            case '!':
                                errHyphenHyphenBang();
                                appendLongStrBuf(c);
                                state = Tokenizer.COMMENT_END_BANG;
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Append two U+002D HYPHEN-MINUS (-) characters
                                 * and the input character to the comment
                                 * token's data.
                                 */
                                adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
                                /*
                                 * Switch to the comment state.
                                 */
                                state = Tokenizer.COMMENT;
                                continue stateloop;
                        }
                    }
                case COMMENT_END_SPACE:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Comment end space state
                         * 
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the comment
                                 * token.
                                 */
                                emitComment(0, pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Switch to the comment
                                 * end dash state.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Switch to the comment end dash state.
                                 */
                                state = Tokenizer.COMMENT_END_DASH;
                                continue stateloop;
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Append the input character to the comment
                                 * token's data. Stay in the comment end space
                                 * state.
                                 */
                                appendLongStrBuf(c);
                                continue;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the input character to
                                 * the comment token's data. Switch to the
                                 * comment state.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Switch to the comment state.
                                 */
                                state = Tokenizer.COMMENT;
                                continue stateloop;
                        }
                    }
                    // XXX reorder point
                case COMMENT_END_BANG:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Comment end bang state
                         * 
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the comment
                                 * token.
                                 */
                                emitComment(3, pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '-':
                                /*
                                 * Append two U+002D HYPHEN-MINUS (-) characters
                                 * and a U+0021 EXCLAMATION MARK (!) character
                                 * to the comment token's data.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Switch to the comment end dash state.
                                 */
                                state = Tokenizer.COMMENT_END_DASH;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append two U+002D HYPHEN-MINUS
                                 * (-) characters, a U+0021 EXCLAMATION MARK (!)
                                 * character, and the input character to the
                                 * comment token's data. Switch to the comment
                                 * state.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Switch to the comment state.
                                 */
                                state = Tokenizer.COMMENT;
                                continue stateloop;
                        }
                    }
                    // XXX reorder point
                case COMMENT_START_DASH:
                    if (++pos == endPos) {
                        break stateloop;
                    }
                    c = checkChar(buf, pos);
                    /*
                     * Comment start dash state
                     * 
                     * Consume the next input character:
                     */
                    switch (c) {
                        case '-':
                            /*
                             * U+002D HYPHEN-MINUS (-) Switch to the comment end
                             * state
                             */
                            appendLongStrBuf(c);
                            state = Tokenizer.COMMENT_END;
                            continue stateloop;
                        case '>':
                            errPrematureEndOfComment();
                            /* Emit the comment token. */
                            emitComment(1, pos);
                            /*
                             * Switch to the data state.
                             */
                            state = Tokenizer.DATA;
                            continue stateloop;
                        case '\r':
                            appendLongStrBufCarriageReturn();
                            state = Tokenizer.COMMENT;
                            break stateloop;
                        case '\n':
                            appendLongStrBufLineFeed();
                            state = Tokenizer.COMMENT;
                            continue stateloop;
                        case '\u0000':
                            c = '\uFFFD';
                            // fall thru
                        default:
                            /*
                             * Append a U+002D HYPHEN-MINUS character (-) and
                             * the current input character to the comment
                             * token's data.
                             */
                            appendLongStrBuf(c);
                            /*
                             * Switch to the comment state.
                             */
                            state = Tokenizer.COMMENT;
                            continue stateloop;
                    }
                    // XXX reorder point
                case CDATA_START:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        if (index < 6) { // CDATA_LSQB.length
                            if (c == Tokenizer.CDATA_LSQB[index]) {
                                appendLongStrBuf(c);
                            } else {
                                errBogusComment();
                                state = Tokenizer.BOGUS_COMMENT;
                                reconsume = true;
                                continue stateloop;
                            }
                            index++;
                            continue;
                        } else {
                            cstart = pos; // start coalescing
                            state = Tokenizer.CDATA_SECTION;
                            reconsume = true;
                            break; // FALL THROUGH continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case CDATA_SECTION:
                    cdatasectionloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        switch (c) {
                            case ']':
                                flushChars(buf, pos);
                                state = Tokenizer.CDATA_RSQB;
                                break cdatasectionloop; // FALL THROUGH
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            default:
                                continue;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case CDATA_RSQB:
                    cdatarsqb: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        switch (c) {
                            case ']':
                                state = Tokenizer.CDATA_RSQB_RSQB;
                                break cdatarsqb;
                            default:
                                tokenHandler.characters(Tokenizer.RSQB_RSQB, 0,
                                        1);
                                cstart = pos;
                                state = Tokenizer.CDATA_SECTION;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case CDATA_RSQB_RSQB:
                    if (++pos == endPos) {
                        break stateloop;
                    }
                    c = checkChar(buf, pos);
                    switch (c) {
                        case '>':
                            cstart = pos + 1;
                            state = Tokenizer.DATA;
                            continue stateloop;
                        default:
                            tokenHandler.characters(Tokenizer.RSQB_RSQB, 0, 2);
                            cstart = pos;
                            state = Tokenizer.CDATA_SECTION;
                            reconsume = true;
                            continue stateloop;

                    }
                    // XXX reorder point
                case ATTRIBUTE_VALUE_SINGLE_QUOTED:
                    attributevaluesinglequotedloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Switch to the after
                                 * attribute value (quoted) state.
                                 */
                                addAttributeWithValue();

                                state = Tokenizer.AFTER_ATTRIBUTE_VALUE_QUOTED;
                                continue stateloop;
                            case '&':
                                /*
                                 * U+0026 AMPERSAND (&) Switch to the character
                                 * reference in attribute value state, with the
                                 * + additional allowed character being U+0027
                                 * APOSTROPHE (').
                                 */
                                clearStrBufAndAppend(c);
                                setAdditionalAndRememberAmpersandLocation('\'');
                                returnState = state;
                                state = Tokenizer.CONSUME_CHARACTER_REFERENCE;
                                break attributevaluesinglequotedloop;
                            // continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the current input
                                 * character to the current attribute's value.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the attribute value (double-quoted)
                                 * state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case CONSUME_CHARACTER_REFERENCE:
                    if (++pos == endPos) {
                        break stateloop;
                    }
                    c = checkChar(buf, pos);
                    if (c == '\u0000') {
                        break stateloop;
                    }
                    /*
                     * Unlike the definition is the spec, this state does not
                     * return a value and never requires the caller to
                     * backtrack. This state takes care of emitting characters
                     * or appending to the current attribute value. It also
                     * takes care of that in the case when consuming the
                     * character reference fails.
                     */
                    /*
                     * This section defines how to consume a character
                     * reference. This definition is used when parsing character
                     * references in text and in attributes.
                     * 
                     * The behavior depends on the identity of the next
                     * character (the one immediately after the U+0026 AMPERSAND
                     * character):
                     */
                    switch (c) {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r': // we'll reconsume!
                        case '\u000C':
                        case '<':
                        case '&':
                            emitOrAppendStrBuf(returnState);
                            if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                cstart = pos;
                            }
                            state = returnState;
                            reconsume = true;
                            continue stateloop;
                        case '#':
                            /*
                             * U+0023 NUMBER SIGN (#) Consume the U+0023 NUMBER
                             * SIGN.
                             */
                            appendStrBuf('#');
                            state = Tokenizer.CONSUME_NCR;
                            continue stateloop;
                        default:
                            if (c == additional) {
                                emitOrAppendStrBuf(returnState);
                                state = returnState;
                                reconsume = true;
                                continue stateloop;
                            }
                            if (c >= 'a' && c <= 'z') {
                                firstCharKey = c - 'a' + 26;
                            } else if (c >= 'A' && c <= 'Z') {
                                firstCharKey = c - 'A';
                            } else {
                                // No match
                                /*
                                 * If no match can be made, then this is a parse
                                 * error.
                                 */
                                errNoNamedCharacterMatch();
                                emitOrAppendStrBuf(returnState);
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos;
                                }
                                state = returnState;
                                reconsume = true;
                                continue stateloop;
                            }
                            // Didn't fail yet
                            appendStrBuf(c);
                            state = Tokenizer.CHARACTER_REFERENCE_HILO_LOOKUP;
                            // FALL THROUGH continue stateloop;
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case CHARACTER_REFERENCE_HILO_LOOKUP:
                    {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        if (c == '\u0000') {
                            break stateloop;
                        }
                        /*
                         * The data structure is as follows:
                         * 
                         * HILO_ACCEL is a two-dimensional int array whose major
                         * index corresponds to the second character of the
                         * character reference (code point as index) and the
                         * minor index corresponds to the first character of the
                         * character reference (packed so that A-Z runs from 0
                         * to 25 and a-z runs from 26 to 51). This layout makes
                         * it easier to use the sparseness of the data structure
                         * to omit parts of it: The second dimension of the
                         * table is null when no character reference starts with
                         * the character corresponding to that row.
                         * 
                         * The int value HILO_ACCEL (by these indeces) is zero
                         * if there exists no character reference starting with
                         * that two-letter prefix. Otherwise, the value is an
                         * int that packs two shorts so that the higher short is
                         * the index of the highest character reference name
                         * with that prefix in NAMES and the lower short
                         * corresponds to the index of the lowest character
                         * reference name with that prefix. (It happens that the
                         * first two character reference names share their
                         * prefix so the packed int cannot be 0 by packing the
                         * two shorts.)
                         * 
                         * NAMES is an array of byte arrays where each byte
                         * array encodes the name of a character references as
                         * ASCII. The names omit the first two letters of the
                         * name. (Since storing the first two letters would be
                         * redundant with the data contained in HILO_ACCEL.) The
                         * entries are lexically sorted.
                         * 
                         * For a given index in NAMES, the same index in VALUES
                         * contains the corresponding expansion as an array of
                         * two UTF-16 code units (either the character and
                         * U+0000 or a suggogate pair).
                         */
                        int hilo = 0;
                        if (c <= 'z') {
                            @Const @NoLength int[] row = NamedCharactersAccel.HILO_ACCEL[c];
                            if (row != null) {
                                hilo = row[firstCharKey];
                            }
                        }
                        if (hilo == 0) {
                            /*
                             * If no match can be made, then this is a parse
                             * error.
                             */
                            errNoNamedCharacterMatch();
                            emitOrAppendStrBuf(returnState);
                            if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                cstart = pos;
                            }
                            state = returnState;
                            reconsume = true;
                            continue stateloop;
                        }
                        // Didn't fail yet
                        appendStrBuf(c);
                        lo = hilo & 0xFFFF;
                        hi = hilo >> 16;
                        entCol = -1;
                        candidate = -1;
                        strBufMark = 0;
                        state = Tokenizer.CHARACTER_REFERENCE_TAIL;
                        // FALL THROUGH continue stateloop;
                    }
                case CHARACTER_REFERENCE_TAIL:
                    outer: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        if (c == '\u0000') {
                            break stateloop;
                        }
                        entCol++;
                        /*
                         * Consume the maximum number of characters possible,
                         * with the consumed characters matching one of the
                         * identifiers in the first column of the named
                         * character references table (in a case-sensitive
                         * manner).
                         */
                        loloop: for (;;) {
                            if (hi < lo) {
                                break outer;
                            }
                            if (entCol == NamedCharacters.NAMES[lo].length) {
                                candidate = lo;
                                strBufMark = strBufLen;
                                lo++;
                            } else if (entCol > NamedCharacters.NAMES[lo].length) {
                                break outer;
                            } else if (c > NamedCharacters.NAMES[lo][entCol]) {
                                lo++;
                            } else {
                                break loloop;
                            }
                        }

                        hiloop: for (;;) {
                            if (hi < lo) {
                                break outer;
                            }
                            if (entCol == NamedCharacters.NAMES[hi].length) {
                                break hiloop;
                            }
                            if (entCol > NamedCharacters.NAMES[hi].length) {
                                break outer;
                            } else if (c < NamedCharacters.NAMES[hi][entCol]) {
                                hi--;
                            } else {
                                break hiloop;
                            }
                        }

                        if (hi < lo) {
                            break outer;
                        }
                        appendStrBuf(c);
                        continue;
                    }

                    // TODO warn about apos (IE) and TRADE (Opera)
                    if (candidate == -1) {
                        // reconsume deals with CR, LF or nul
                        /*
                         * If no match can be made, then this is a parse error.
                         */
                        errNoNamedCharacterMatch();
                        emitOrAppendStrBuf(returnState);
                        if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                            cstart = pos;
                        }
                        state = returnState;
                        reconsume = true;
                        continue stateloop;
                    } else {
                        // c can't be CR, LF or nul if we got here
                        byte[] candidateArr = NamedCharacters.NAMES[candidate];
                        if (candidateArr.length == 0
                                || candidateArr[candidateArr.length - 1] != ';') {
                            /*
                             * If the last character matched is not a U+003B
                             * SEMICOLON (;), there is a parse error.
                             */
                            if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
                                /*
                                 * If the entity is being consumed as part of an
                                 * attribute, and the last character matched is
                                 * not a U+003B SEMICOLON (;),
                                 */
                                char ch;
                                if (strBufMark == strBufLen) {
                                    ch = c;
                                } else {
                                    // if (strBufOffset != -1) {
                                    // ch = buf[strBufOffset + strBufMark];
                                    // } else {
                                    ch = strBuf[strBufMark];
                                    // }
                                }
                                if (ch == '=' || (ch >= '0' && ch <= '9')
                                        || (ch >= 'A' && ch <= 'Z')
                                        || (ch >= 'a' && ch <= 'z')) {
                                    /*
                                     * and the next character is either a U+003D
                                     * EQUALS SIGN character (=) or in the range
                                     * U+0030 DIGIT ZERO to U+0039 DIGIT NINE,
                                     * U+0041 LATIN CAPITAL LETTER A to U+005A
                                     * LATIN CAPITAL LETTER Z, or U+0061 LATIN
                                     * SMALL LETTER A to U+007A LATIN SMALL
                                     * LETTER Z, then, for historical reasons,
                                     * all the characters that were matched
                                     * after the U+0026 AMPERSAND (&) must be
                                     * unconsumed, and nothing is returned.
                                     */
                                    errNoNamedCharacterMatch();
                                    appendStrBufToLongStrBuf();
                                    state = returnState;
                                    reconsume = true;
                                    continue stateloop;
                                }
                            }
                            if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
                                errUnescapedAmpersandInterpretedAsCharacterReference();
                            } else {
                                errNotSemicolonTerminated();
                            }
                        }

                        /*
                         * Otherwise, return a character token for the character
                         * corresponding to the entity name (as given by the
                         * second column of the named character references
                         * table).
                         */
                        @Const @NoLength char[] val = NamedCharacters.VALUES[candidate];
                        // See if the first slot holds a high surrogate
                        if ((val[0] & 0xFC00) == 0xD800) {
                            emitOrAppendTwo(val, returnState);
                        } else {
                            emitOrAppendOne(val, returnState);
                        }
                        // this is so complicated!
                        if (strBufMark < strBufLen) {
                            // if (strBufOffset != -1) {
                            // if ((returnState & (~1)) != 0) {
                            // for (int i = strBufMark; i < strBufLen; i++) {
                            // appendLongStrBuf(buf[strBufOffset + i]);
                            // }
                            // } else {
                            // tokenHandler.characters(buf, strBufOffset
                            // + strBufMark, strBufLen
                            // - strBufMark);
                            // }
                            // } else {
                            if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
                                for (int i = strBufMark; i < strBufLen; i++) {
                                    appendLongStrBuf(strBuf[i]);
                                }
                            } else {
                                tokenHandler.characters(strBuf, strBufMark,
                                        strBufLen - strBufMark);
                            }
                            // }
                        }
                        if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                            cstart = pos;
                        }
                        state = returnState;
                        reconsume = true;
                        continue stateloop;
                        /*
                         * If the markup contains I'm &notit; I tell you, the
                         * entity is parsed as "not", as in, I'm ¬it; I tell
                         * you. But if the markup was I'm &notin; I tell you,
                         * the entity would be parsed as "notin;", resulting in
                         * I'm ∉ I tell you.
                         */
                    }
                    // XXX reorder point
                case CONSUME_NCR:
                    if (++pos == endPos) {
                        break stateloop;
                    }
                    c = checkChar(buf, pos);
                    prevValue = -1;
                    value = 0;
                    seenDigits = false;
                    /*
                     * The behavior further depends on the character after the
                     * U+0023 NUMBER SIGN:
                     */
                    switch (c) {
                        case 'x':
                        case 'X':

                            /*
                             * U+0078 LATIN SMALL LETTER X U+0058 LATIN CAPITAL
                             * LETTER X Consume the X.
                             * 
                             * Follow the steps below, but using the range of
                             * characters U+0030 DIGIT ZERO through to U+0039
                             * DIGIT NINE, U+0061 LATIN SMALL LETTER A through
                             * to U+0066 LATIN SMALL LETTER F, and U+0041 LATIN
                             * CAPITAL LETTER A, through to U+0046 LATIN CAPITAL
                             * LETTER F (in other words, 0-9, A-F, a-f).
                             * 
                             * When it comes to interpreting the number,
                             * interpret it as a hexadecimal number.
                             */
                            appendStrBuf(c);
                            state = Tokenizer.HEX_NCR_LOOP;
                            continue stateloop;
                        default:
                            /*
                             * Anything else Follow the steps below, but using
                             * the range of characters U+0030 DIGIT ZERO through
                             * to U+0039 DIGIT NINE (i.e. just 0-9).
                             * 
                             * When it comes to interpreting the number,
                             * interpret it as a decimal number.
                             */
                            state = Tokenizer.DECIMAL_NRC_LOOP;
                            reconsume = true;
                            // FALL THROUGH continue stateloop;
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case DECIMAL_NRC_LOOP:
                    decimalloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        // Deal with overflow gracefully
                        if (value < prevValue) {
                            value = 0x110000; // Value above Unicode range but
                            // within int
                            // range
                        }
                        prevValue = value;
                        /*
                         * Consume as many characters as match the range of
                         * characters given above.
                         */
                        if (c >= '0' && c <= '9') {
                            seenDigits = true;
                            value *= 10;
                            value += c - '0';
                            continue;
                        } else if (c == ';') {
                            if (seenDigits) {
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos + 1;
                                }
                                state = Tokenizer.HANDLE_NCR_VALUE;
                                // FALL THROUGH continue stateloop;
                                break decimalloop;
                            } else {
                                errNoDigitsInNCR();
                                appendStrBuf(';');
                                emitOrAppendStrBuf(returnState);
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos + 1;
                                }
                                state = returnState;
                                continue stateloop;
                            }
                        } else {
                            /*
                             * If no characters match the range, then don't
                             * consume any characters (and unconsume the U+0023
                             * NUMBER SIGN character and, if appropriate, the X
                             * character). This is a parse error; nothing is
                             * returned.
                             * 
                             * Otherwise, if the next character is a U+003B
                             * SEMICOLON, consume that too. If it isn't, there
                             * is a parse error.
                             */
                            if (!seenDigits) {
                                errNoDigitsInNCR();
                                emitOrAppendStrBuf(returnState);
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos;
                                }
                                state = returnState;
                                reconsume = true;
                                continue stateloop;
                            } else {
                                errCharRefLacksSemicolon();
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos;
                                }
                                state = Tokenizer.HANDLE_NCR_VALUE;
                                reconsume = true;
                                // FALL THROUGH continue stateloop;
                                break decimalloop;
                            }
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case HANDLE_NCR_VALUE:
                    // WARNING previous state sets reconsume
                    // XXX inline this case if the method size can take it
                    handleNcrValue(returnState);
                    state = returnState;
                    continue stateloop;
                    // XXX reorder point
                case HEX_NCR_LOOP:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        // Deal with overflow gracefully
                        if (value < prevValue) {
                            value = 0x110000; // Value above Unicode range but
                            // within int
                            // range
                        }
                        prevValue = value;
                        /*
                         * Consume as many characters as match the range of
                         * characters given above.
                         */
                        if (c >= '0' && c <= '9') {
                            seenDigits = true;
                            value *= 16;
                            value += c - '0';
                            continue;
                        } else if (c >= 'A' && c <= 'F') {
                            seenDigits = true;
                            value *= 16;
                            value += c - 'A' + 10;
                            continue;
                        } else if (c >= 'a' && c <= 'f') {
                            seenDigits = true;
                            value *= 16;
                            value += c - 'a' + 10;
                            continue;
                        } else if (c == ';') {
                            if (seenDigits) {
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos + 1;
                                }
                                state = Tokenizer.HANDLE_NCR_VALUE;
                                continue stateloop;
                            } else {
                                errNoDigitsInNCR();
                                appendStrBuf(';');
                                emitOrAppendStrBuf(returnState);
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos + 1;
                                }
                                state = returnState;
                                continue stateloop;
                            }
                        } else {
                            /*
                             * If no characters match the range, then don't
                             * consume any characters (and unconsume the U+0023
                             * NUMBER SIGN character and, if appropriate, the X
                             * character). This is a parse error; nothing is
                             * returned.
                             * 
                             * Otherwise, if the next character is a U+003B
                             * SEMICOLON, consume that too. If it isn't, there
                             * is a parse error.
                             */
                            if (!seenDigits) {
                                errNoDigitsInNCR();
                                emitOrAppendStrBuf(returnState);
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos;
                                }
                                state = returnState;
                                reconsume = true;
                                continue stateloop;
                            } else {
                                errCharRefLacksSemicolon();
                                if ((returnState & DATA_AND_RCDATA_MASK) == 0) {
                                    cstart = pos;
                                }
                                state = Tokenizer.HANDLE_NCR_VALUE;
                                reconsume = true;
                                continue stateloop;
                            }
                        }
                    }
                    // XXX reorder point
                case PLAINTEXT:
                    plaintextloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        switch (c) {
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Stay in the
                                 * RAWTEXT state.
                                 */
                                continue;
                        }
                    }
                    // XXX reorder point
                case CLOSE_TAG_OPEN:
                    if (++pos == endPos) {
                        break stateloop;
                    }
                    c = checkChar(buf, pos);
                    /*
                     * Otherwise, if the content model flag is set to the PCDATA
                     * state, or if the next few characters do match that tag
                     * name, consume the next input character:
                     */
                    switch (c) {
                        case '>':
                            /* U+003E GREATER-THAN SIGN (>) Parse error. */
                            errLtSlashGt();
                            /*
                             * Switch to the data state.
                             */
                            cstart = pos + 1;
                            state = Tokenizer.DATA;
                            continue stateloop;
                        case '\r':
                            silentCarriageReturn();
                            /* Anything else Parse error. */
                            errGarbageAfterLtSlash();
                            /*
                             * Switch to the bogus comment state.
                             */
                            clearLongStrBufAndAppend('\n');
                            state = Tokenizer.BOGUS_COMMENT;
                            break stateloop;
                        case '\n':
                            silentLineFeed();
                            /* Anything else Parse error. */
                            errGarbageAfterLtSlash();
                            /*
                             * Switch to the bogus comment state.
                             */
                            clearLongStrBufAndAppend('\n');
                            state = Tokenizer.BOGUS_COMMENT;
                            continue stateloop;
                        case '\u0000':
                            c = '\uFFFD';
                            // fall thru
                        default:
                            if (c >= 'A' && c <= 'Z') {
                                c += 0x20;
                            }
                            if (c >= 'a' && c <= 'z') {
                                /*
                                 * U+0061 LATIN SMALL LETTER A through to U+007A
                                 * LATIN SMALL LETTER Z Create a new end tag
                                 * token,
                                 */
                                endTag = true;
                                /*
                                 * set its tag name to the input character,
                                 */
                                clearStrBufAndAppend(c);
                                /*
                                 * then switch to the tag name state. (Don't
                                 * emit the token yet; further details will be
                                 * filled in before it is emitted.)
                                 */
                                state = Tokenizer.TAG_NAME;
                                continue stateloop;
                            } else {
                                /* Anything else Parse error. */
                                errGarbageAfterLtSlash();
                                /*
                                 * Switch to the bogus comment state.
                                 */
                                clearLongStrBufAndAppend(c);
                                state = Tokenizer.BOGUS_COMMENT;
                                continue stateloop;
                            }
                    }
                    // XXX reorder point
                case RCDATA:
                    rcdataloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        switch (c) {
                            case '&':
                                /*
                                 * U+0026 AMPERSAND (&) Switch to the character
                                 * reference in RCDATA state.
                                 */
                                flushChars(buf, pos);
                                clearStrBufAndAppend(c);
                                additional = '\u0000';
                                returnState = state;
                                state = Tokenizer.CONSUME_CHARACTER_REFERENCE;
                                continue stateloop;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the
                                 * RCDATA less-than sign state.
                                 */
                                flushChars(buf, pos);

                                returnState = state;
                                state = Tokenizer.RAWTEXT_RCDATA_LESS_THAN_SIGN;
                                continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Emit the current input character as a
                                 * character token. Stay in the RCDATA state.
                                 */
                                continue;
                        }
                    }
                    // XXX reorder point
                case RAWTEXT:
                    rawtextloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        switch (c) {
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the
                                 * RAWTEXT less-than sign state.
                                 */
                                flushChars(buf, pos);

                                returnState = state;
                                state = Tokenizer.RAWTEXT_RCDATA_LESS_THAN_SIGN;
                                break rawtextloop;
                            // FALL THRU continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Emit the current input character as a
                                 * character token. Stay in the RAWTEXT state.
                                 */
                                continue;
                        }
                    }
                    // XXX fallthru don't reorder
                case RAWTEXT_RCDATA_LESS_THAN_SIGN:
                    rawtextrcdatalessthansignloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        switch (c) {
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Set the temporary buffer
                                 * to the empty string. Switch to the script
                                 * data end tag open state.
                                 */
                                index = 0;
                                clearStrBuf();
                                state = Tokenizer.NON_DATA_END_TAG_NAME;
                                break rawtextrcdatalessthansignloop;
                            // FALL THRU continue stateloop;
                            default:
                                /*
                                 * Otherwise, emit a U+003C LESS-THAN SIGN
                                 * character token
                                 */
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                                /*
                                 * and reconsume the current input character in
                                 * the data state.
                                 */
                                cstart = pos;
                                state = returnState;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // XXX fall thru. don't reorder.
                case NON_DATA_END_TAG_NAME:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * ASSERT! when entering this state, set index to 0 and
                         * call clearStrBuf() assert (contentModelElement !=
                         * null); Let's implement the above without lookahead.
                         * strBuf is the 'temporary buffer'.
                         */
                        if (index < endTagExpectationAsArray.length) {
                            char e = endTagExpectationAsArray[index];
                            char folded = c;
                            if (c >= 'A' && c <= 'Z') {
                                folded += 0x20;
                            }
                            if (folded != e) {
                                // [NOCPP[
                                errHtml4LtSlashInRcdata(folded);
                                // ]NOCPP]
                                tokenHandler.characters(Tokenizer.LT_SOLIDUS,
                                        0, 2);
                                emitStrBuf();
                                cstart = pos;
                                state = returnState;
                                reconsume = true;
                                continue stateloop;
                            }
                            appendStrBuf(c);
                            index++;
                            continue;
                        } else {
                            endTag = true;
                            // XXX replace contentModelElement with different
                            // type
                            tagName = endTagExpectation;
                            switch (c) {
                                case '\r':
                                    silentCarriageReturn();
                                    state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                    break stateloop;
                                case '\n':
                                    silentLineFeed();
                                    // fall thru
                                case ' ':
                                case '\t':
                                case '\u000C':
                                    /*
                                     * U+0009 CHARACTER TABULATION U+000A LINE
                                     * FEED (LF) U+000C FORM FEED (FF) U+0020
                                     * SPACE If the current end tag token is an
                                     * appropriate end tag token, then switch to
                                     * the before attribute name state.
                                     */
                                    state = Tokenizer.BEFORE_ATTRIBUTE_NAME;
                                    continue stateloop;
                                case '/':
                                    /*
                                     * U+002F SOLIDUS (/) If the current end tag
                                     * token is an appropriate end tag token,
                                     * then switch to the self-closing start tag
                                     * state.
                                     */
                                    state = Tokenizer.SELF_CLOSING_START_TAG;
                                    continue stateloop;
                                case '>':
                                    /*
                                     * U+003E GREATER-THAN SIGN (>) If the
                                     * current end tag token is an appropriate
                                     * end tag token, then emit the current tag
                                     * token and switch to the data state.
                                     */
                                    state = emitCurrentTagToken(false, pos);
                                    if (shouldSuspend) {
                                        break stateloop;
                                    }
                                    continue stateloop;
                                default:
                                    /*
                                     * Emit a U+003C LESS-THAN SIGN character
                                     * token, a U+002F SOLIDUS character token,
                                     * a character token for each of the
                                     * characters in the temporary buffer (in
                                     * the order they were added to the buffer),
                                     * and reconsume the current input character
                                     * in the RAWTEXT state.
                                     */
                                    // [NOCPP[
                                    errWarnLtSlashInRcdata();
                                    // ]NOCPP]
                                    tokenHandler.characters(
                                            Tokenizer.LT_SOLIDUS, 0, 2);
                                    emitStrBuf();
                                    if (c == '\u0000') {
                                        emitReplacementCharacter(buf, pos);
                                    } else {
                                        cstart = pos; // don't drop the
                                        // character
                                    }
                                    state = returnState;
                                    continue stateloop;
                            }
                        }
                    }
                    // XXX reorder point
                    // BEGIN HOTSPOT WORKAROUND
                case BOGUS_COMMENT:
                    boguscommentloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume every character up to and including the first
                         * U+003E GREATER-THAN SIGN character (>) or the end of
                         * the file (EOF), whichever comes first. Emit a comment
                         * token whose data is the concatenation of all the
                         * characters starting from and including the character
                         * that caused the state machine to switch into the
                         * bogus comment state, up to and including the
                         * character immediately before the last consumed
                         * character (i.e. up to the character just before the
                         * U+003E or EOF character). (If the comment was started
                         * by the end of the file (EOF), the token is empty.)
                         * 
                         * Switch to the data state.
                         * 
                         * If the end of the file was reached, reconsume the EOF
                         * character.
                         */
                        switch (c) {
                            case '>':
                                emitComment(0, pos);
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '-':
                                appendLongStrBuf(c);
                                state = Tokenizer.BOGUS_COMMENT_HYPHEN;
                                break boguscommentloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                appendLongStrBuf(c);
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BOGUS_COMMENT_HYPHEN:
                    boguscommenthyphenloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        switch (c) {
                            case '>':
                                // [NOCPP[
                                maybeAppendSpaceToBogusComment();
                                // ]NOCPP]
                                emitComment(0, pos);
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '-':
                                appendSecondHyphenToBogusComment();
                                continue boguscommenthyphenloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                state = Tokenizer.BOGUS_COMMENT;
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                state = Tokenizer.BOGUS_COMMENT;
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                appendLongStrBuf(c);
                                state = Tokenizer.BOGUS_COMMENT;
                                continue stateloop;
                        }
                    }
                    // XXX reorder point
                case SCRIPT_DATA:
                    scriptdataloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        switch (c) {
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the
                                 * script data less-than sign state.
                                 */
                                flushChars(buf, pos);
                                returnState = state;
                                state = Tokenizer.SCRIPT_DATA_LESS_THAN_SIGN;
                                break scriptdataloop; // FALL THRU continue
                            // stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Stay in the
                                 * script data state.
                                 */
                                continue;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_LESS_THAN_SIGN:
                    scriptdatalessthansignloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        switch (c) {
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Set the temporary buffer
                                 * to the empty string. Switch to the script
                                 * data end tag open state.
                                 */
                                index = 0;
                                clearStrBuf();
                                state = Tokenizer.NON_DATA_END_TAG_NAME;
                                continue stateloop;
                            case '!':
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                                cstart = pos;
                                state = Tokenizer.SCRIPT_DATA_ESCAPE_START;
                                break scriptdatalessthansignloop; // FALL THRU
                            // continue
                            // stateloop;
                            default:
                                /*
                                 * Otherwise, emit a U+003C LESS-THAN SIGN
                                 * character token
                                 */
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                                /*
                                 * and reconsume the current input character in
                                 * the data state.
                                 */
                                cstart = pos;
                                state = Tokenizer.SCRIPT_DATA;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_ESCAPE_START:
                    scriptdataescapestartloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Switch to the
                                 * script data escape start dash state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPE_START_DASH;
                                break scriptdataescapestartloop; // FALL THRU
                            // continue
                            // stateloop;
                            default:
                                /*
                                 * Anything else Reconsume the current input
                                 * character in the script data state.
                                 */
                                state = Tokenizer.SCRIPT_DATA;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_ESCAPE_START_DASH:
                    scriptdataescapestartdashloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Switch to the
                                 * script data escaped dash dash state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPED_DASH_DASH;
                                break scriptdataescapestartdashloop;
                            // continue stateloop;
                            default:
                                /*
                                 * Anything else Reconsume the current input
                                 * character in the script data state.
                                 */
                                state = Tokenizer.SCRIPT_DATA;
                                reconsume = true;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_ESCAPED_DASH_DASH:
                    scriptdataescapeddashdashloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Stay in the
                                 * script data escaped dash dash state.
                                 */
                                continue;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the
                                 * script data escaped less-than sign state.
                                 */
                                flushChars(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit a U+003E
                                 * GREATER-THAN SIGN character token. Switch to
                                 * the script data state.
                                 */
                                state = Tokenizer.SCRIPT_DATA;
                                continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                break scriptdataescapeddashdashloop;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Switch to the
                                 * script data escaped state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                break scriptdataescapeddashdashloop;
                            // continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_ESCAPED:
                    scriptdataescapedloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Switch to the
                                 * script data escaped dash state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPED_DASH;
                                break scriptdataescapedloop; // FALL THRU
                            // continue
                            // stateloop;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the
                                 * script data escaped less-than sign state.
                                 */
                                flushChars(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN;
                                continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Stay in the
                                 * script data escaped state.
                                 */
                                continue;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_ESCAPED_DASH:
                    scriptdataescapeddashloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Switch to the
                                 * script data escaped dash dash state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPED_DASH_DASH;
                                continue stateloop;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Switch to the
                                 * script data escaped less-than sign state.
                                 */
                                flushChars(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN;
                                break scriptdataescapeddashloop;
                            // continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                continue stateloop;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Switch to the
                                 * script data escaped state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN:
                    scriptdataescapedlessthanloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Set the temporary buffer
                                 * to the empty string. Switch to the script
                                 * data escaped end tag open state.
                                 */
                                index = 0;
                                clearStrBuf();
                                returnState = Tokenizer.SCRIPT_DATA_ESCAPED;
                                state = Tokenizer.NON_DATA_END_TAG_NAME;
                                continue stateloop;
                            case 'S':
                            case 's':
                                /*
                                 * U+0041 LATIN CAPITAL LETTER A through to
                                 * U+005A LATIN CAPITAL LETTER Z Emit a U+003C
                                 * LESS-THAN SIGN character token and the
                                 * current input character as a character token.
                                 */
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                                cstart = pos;
                                index = 1;
                                /*
                                 * Set the temporary buffer to the empty string.
                                 * Append the lowercase version of the current
                                 * input character (add 0x0020 to the
                                 * character's code point) to the temporary
                                 * buffer. Switch to the script data double
                                 * escape start state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPE_START;
                                break scriptdataescapedlessthanloop;
                            // continue stateloop;
                            default:
                                /*
                                 * Anything else Emit a U+003C LESS-THAN SIGN
                                 * character token and reconsume the current
                                 * input character in the script data escaped
                                 * state.
                                 */
                                tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                                cstart = pos;
                                reconsume = true;
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_DOUBLE_ESCAPE_START:
                    scriptdatadoubleescapestartloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        assert (index > 0);
                        if (index < 6) { // SCRIPT_ARR.length
                            char folded = c;
                            if (c >= 'A' && c <= 'Z') {
                                folded += 0x20;
                            }
                            if (folded != Tokenizer.SCRIPT_ARR[index]) {
                                reconsume = true;
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                continue stateloop;
                            }
                            index++;
                            continue;
                        }
                        switch (c) {
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            case ' ':
                            case '\t':
                            case '\u000C':
                            case '/':
                            case '>':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * U+002F SOLIDUS (/) U+003E GREATER-THAN SIGN
                                 * (>) Emit the current input character as a
                                 * character token. If the temporary buffer is
                                 * the string "script", then switch to the
                                 * script data double escaped state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                break scriptdatadoubleescapestartloop;
                            // continue stateloop;
                            default:
                                /*
                                 * Anything else Reconsume the current input
                                 * character in the script data escaped state.
                                 */
                                reconsume = true;
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_DOUBLE_ESCAPED:
                    scriptdatadoubleescapedloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Switch to the
                                 * script data double escaped dash state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED_DASH;
                                break scriptdatadoubleescapedloop; // FALL THRU
                            // continue
                            // stateloop;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Emit a U+003C
                                 * LESS-THAN SIGN character token. Switch to the
                                 * script data double escaped less-than sign
                                 * state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN;
                                continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                continue;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Stay in the
                                 * script data double escaped state.
                                 */
                                continue;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
                    scriptdatadoubleescapeddashloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Switch to the
                                 * script data double escaped dash dash state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH;
                                break scriptdatadoubleescapeddashloop;
                            // continue stateloop;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Emit a U+003C
                                 * LESS-THAN SIGN character token. Switch to the
                                 * script data double escaped less-than sign
                                 * state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN;
                                continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Switch to the
                                 * script data double escaped state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
                    scriptdatadoubleescapeddashdashloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '-':
                                /*
                                 * U+002D HYPHEN-MINUS (-) Emit a U+002D
                                 * HYPHEN-MINUS character token. Stay in the
                                 * script data double escaped dash dash state.
                                 */
                                continue;
                            case '<':
                                /*
                                 * U+003C LESS-THAN SIGN (<) Emit a U+003C
                                 * LESS-THAN SIGN character token. Switch to the
                                 * script data double escaped less-than sign
                                 * state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN;
                                break scriptdatadoubleescapeddashdashloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit a U+003E
                                 * GREATER-THAN SIGN character token. Switch to
                                 * the script data state.
                                 */
                                state = Tokenizer.SCRIPT_DATA;
                                continue stateloop;
                            case '\u0000':
                                emitReplacementCharacter(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            default:
                                /*
                                 * Anything else Emit the current input
                                 * character as a character token. Switch to the
                                 * script data double escaped state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN:
                    scriptdatadoubleescapedlessthanloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '/':
                                /*
                                 * U+002F SOLIDUS (/) Emit a U+002F SOLIDUS
                                 * character token. Set the temporary buffer to
                                 * the empty string. Switch to the script data
                                 * double escape end state.
                                 */
                                index = 0;
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPE_END;
                                break scriptdatadoubleescapedlessthanloop;
                            default:
                                /*
                                 * Anything else Reconsume the current input
                                 * character in the script data double escaped
                                 * state.
                                 */
                                reconsume = true;
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // WARNING FALLTHRU CASE TRANSITION: DON'T REORDER
                case SCRIPT_DATA_DOUBLE_ESCAPE_END:
                    scriptdatadoubleescapeendloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        if (index < 6) { // SCRIPT_ARR.length
                            char folded = c;
                            if (c >= 'A' && c <= 'Z') {
                                folded += 0x20;
                            }
                            if (folded != Tokenizer.SCRIPT_ARR[index]) {
                                reconsume = true;
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                            }
                            index++;
                            continue;
                        }
                        switch (c) {
                            case '\r':
                                emitCarriageReturn(buf, pos);
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                            case ' ':
                            case '\t':
                            case '\u000C':
                            case '/':
                            case '>':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * U+002F SOLIDUS (/) U+003E GREATER-THAN SIGN
                                 * (>) Emit the current input character as a
                                 * character token. If the temporary buffer is
                                 * the string "script", then switch to the
                                 * script data escaped state.
                                 */
                                state = Tokenizer.SCRIPT_DATA_ESCAPED;
                                continue stateloop;
                            default:
                                /*
                                 * Reconsume the current input character in the
                                 * script data double escaped state.
                                 */
                                reconsume = true;
                                state = Tokenizer.SCRIPT_DATA_DOUBLE_ESCAPED;
                                continue stateloop;
                        }
                    }
                    // XXX reorder point
                case MARKUP_DECLARATION_OCTYPE:
                    markupdeclarationdoctypeloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        if (index < 6) { // OCTYPE.length
                            char folded = c;
                            if (c >= 'A' && c <= 'Z') {
                                folded += 0x20;
                            }
                            if (folded == Tokenizer.OCTYPE[index]) {
                                appendLongStrBuf(c);
                            } else {
                                errBogusComment();
                                state = Tokenizer.BOGUS_COMMENT;
                                reconsume = true;
                                continue stateloop;
                            }
                            index++;
                            continue;
                        } else {
                            state = Tokenizer.DOCTYPE;
                            reconsume = true;
                            break markupdeclarationdoctypeloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case DOCTYPE:
                    doctypeloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        initDoctypeFields();
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                state = Tokenizer.BEFORE_DOCTYPE_NAME;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the before DOCTYPE name state.
                                 */
                                state = Tokenizer.BEFORE_DOCTYPE_NAME;
                                break doctypeloop;
                            // continue stateloop;
                            default:
                                /*
                                 * Anything else Parse error.
                                 */
                                errMissingSpaceBeforeDoctypeName();
                                /*
                                 * Reconsume the current character in the before
                                 * DOCTYPE name state.
                                 */
                                state = Tokenizer.BEFORE_DOCTYPE_NAME;
                                reconsume = true;
                                break doctypeloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BEFORE_DOCTYPE_NAME:
                    beforedoctypenameloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the before DOCTYPE name state.
                                 */
                                continue;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Parse error.
                                 */
                                errNamelessDoctype();
                                /*
                                 * Create a new DOCTYPE token. Set its
                                 * force-quirks flag to on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit the token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                if (c >= 'A' && c <= 'Z') {
                                    /*
                                     * U+0041 LATIN CAPITAL LETTER A through to
                                     * U+005A LATIN CAPITAL LETTER Z Create a
                                     * new DOCTYPE token. Set the token's name
                                     * to the lowercase version of the input
                                     * character (add 0x0020 to the character's
                                     * code point).
                                     */
                                    c += 0x20;
                                }
                                /* Anything else Create a new DOCTYPE token. */
                                /*
                                 * Set the token's name name to the current
                                 * input character.
                                 */
                                clearStrBufAndAppend(c);
                                /*
                                 * Switch to the DOCTYPE name state.
                                 */
                                state = Tokenizer.DOCTYPE_NAME;
                                break beforedoctypenameloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case DOCTYPE_NAME:
                    doctypenameloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                strBufToDoctypeName();
                                state = Tokenizer.AFTER_DOCTYPE_NAME;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the after DOCTYPE name state.
                                 */
                                strBufToDoctypeName();
                                state = Tokenizer.AFTER_DOCTYPE_NAME;
                                break doctypenameloop;
                            // continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * DOCTYPE token.
                                 */
                                strBufToDoctypeName();
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * U+0041 LATIN CAPITAL LETTER A through to
                                 * U+005A LATIN CAPITAL LETTER Z Append the
                                 * lowercase version of the input character (add
                                 * 0x0020 to the character's code point) to the
                                 * current DOCTYPE token's name.
                                 */
                                if (c >= 'A' && c <= 'Z') {
                                    c += 0x0020;
                                }
                                /*
                                 * Anything else Append the current input
                                 * character to the current DOCTYPE token's
                                 * name.
                                 */
                                appendStrBuf(c);
                                /*
                                 * Stay in the DOCTYPE name state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case AFTER_DOCTYPE_NAME:
                    afterdoctypenameloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the after DOCTYPE name state.
                                 */
                                continue;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case 'p':
                            case 'P':
                                index = 0;
                                state = Tokenizer.DOCTYPE_UBLIC;
                                break afterdoctypenameloop;
                            // continue stateloop;
                            case 's':
                            case 'S':
                                index = 0;
                                state = Tokenizer.DOCTYPE_YSTEM;
                                continue stateloop;
                            default:
                                /*
                                 * Otherwise, this is the parse error.
                                 */
                                bogusDoctype();

                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case DOCTYPE_UBLIC:
                    doctypeublicloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * If the six characters starting from the current input
                         * character are an ASCII case-insensitive match for the
                         * word "PUBLIC", then consume those characters and
                         * switch to the before DOCTYPE public identifier state.
                         */
                        if (index < 5) { // UBLIC.length
                            char folded = c;
                            if (c >= 'A' && c <= 'Z') {
                                folded += 0x20;
                            }
                            if (folded != Tokenizer.UBLIC[index]) {
                                bogusDoctype();
                                // forceQuirks = true;
                                state = Tokenizer.BOGUS_DOCTYPE;
                                reconsume = true;
                                continue stateloop;
                            }
                            index++;
                            continue;
                        } else {
                            state = Tokenizer.AFTER_DOCTYPE_PUBLIC_KEYWORD;
                            reconsume = true;
                            break doctypeublicloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case AFTER_DOCTYPE_PUBLIC_KEYWORD:
                    afterdoctypepublickeywordloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                state = Tokenizer.BEFORE_DOCTYPE_PUBLIC_IDENTIFIER;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the before DOCTYPE public
                                 * identifier state.
                                 */
                                state = Tokenizer.BEFORE_DOCTYPE_PUBLIC_IDENTIFIER;
                                break afterdoctypepublickeywordloop;
                            // FALL THROUGH continue stateloop
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Parse Error.
                                 */
                                errNoSpaceBetweenDoctypePublicKeywordAndQuote();
                                /*
                                 * Set the DOCTYPE token's public identifier to
                                 * the empty string (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE public identifier
                                 * (double-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED;
                                continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Parse Error.
                                 */
                                errNoSpaceBetweenDoctypePublicKeywordAndQuote();
                                /*
                                 * Set the DOCTYPE token's public identifier to
                                 * the empty string (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE public identifier
                                 * (single-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED;
                                continue stateloop;
                            case '>':
                                /* U+003E GREATER-THAN SIGN (>) Parse error. */
                                errExpectedPublicId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            default:
                                bogusDoctype();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
                    beforedoctypepublicidentifierloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the before DOCTYPE public identifier
                                 * state.
                                 */
                                continue;
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Set the DOCTYPE
                                 * token's public identifier to the empty string
                                 * (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE public identifier
                                 * (double-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED;
                                break beforedoctypepublicidentifierloop;
                            // continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Set the DOCTYPE token's
                                 * public identifier to the empty string (not
                                 * missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE public identifier
                                 * (single-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED;
                                continue stateloop;
                            case '>':
                                /* U+003E GREATER-THAN SIGN (>) Parse error. */
                                errExpectedPublicId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            default:
                                bogusDoctype();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
                    doctypepublicidentifierdoublequotedloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Switch to the after
                                 * DOCTYPE public identifier state.
                                 */
                                publicIdentifier = longStrBufToString();
                                state = Tokenizer.AFTER_DOCTYPE_PUBLIC_IDENTIFIER;
                                break doctypepublicidentifierdoublequotedloop;
                            // continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Parse error.
                                 */
                                errGtInPublicId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                publicIdentifier = longStrBufToString();
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the current input
                                 * character to the current DOCTYPE token's
                                 * public identifier.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the DOCTYPE public identifier
                                 * (double-quoted) state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
                    afterdoctypepublicidentifierloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                state = Tokenizer.BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the between DOCTYPE public and
                                 * system identifiers state.
                                 */
                                state = Tokenizer.BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS;
                                break afterdoctypepublicidentifierloop;
                            // continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Parse error.
                                 */
                                errNoSpaceBetweenPublicAndSystemIds();
                                /*
                                 * Set the DOCTYPE token's system identifier to
                                 * the empty string (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE system identifier
                                 * (double-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
                                continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Parse error.
                                 */
                                errNoSpaceBetweenPublicAndSystemIds();
                                /*
                                 * Set the DOCTYPE token's system identifier to
                                 * the empty string (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE system identifier
                                 * (single-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
                                continue stateloop;
                            default:
                                bogusDoctype();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
                    betweendoctypepublicandsystemidentifiersloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the between DOCTYPE public and system
                                 * identifiers state.
                                 */
                                continue;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Set the DOCTYPE
                                 * token's system identifier to the empty string
                                 * (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE system identifier
                                 * (double-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
                                break betweendoctypepublicandsystemidentifiersloop;
                            // continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Set the DOCTYPE token's
                                 * system identifier to the empty string (not
                                 * missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE system identifier
                                 * (single-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
                                continue stateloop;
                            default:
                                bogusDoctype();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
                    doctypesystemidentifierdoublequotedloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Switch to the after
                                 * DOCTYPE system identifier state.
                                 */
                                systemIdentifier = longStrBufToString();
                                state = Tokenizer.AFTER_DOCTYPE_SYSTEM_IDENTIFIER;
                                continue stateloop;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Parse error.
                                 */
                                errGtInSystemId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                systemIdentifier = longStrBufToString();
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the current input
                                 * character to the current DOCTYPE token's
                                 * system identifier.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the DOCTYPE system identifier
                                 * (double-quoted) state.
                                 */
                                continue;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
                    afterdoctypesystemidentifierloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the after DOCTYPE system identifier state.
                                 */
                                continue;
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit the current
                                 * DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            default:
                                /*
                                 * Switch to the bogus DOCTYPE state. (This does
                                 * not set the DOCTYPE token's force-quirks flag
                                 * to on.)
                                 */
                                bogusDoctypeWithoutQuirks();
                                state = Tokenizer.BOGUS_DOCTYPE;
                                break afterdoctypesystemidentifierloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BOGUS_DOCTYPE:
                    for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '>':
                                /*
                                 * U+003E GREATER-THAN SIGN (>) Emit that
                                 * DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            default:
                                /*
                                 * Anything else Stay in the bogus DOCTYPE
                                 * state.
                                 */
                                continue;
                        }
                    }
                    // XXX reorder point
                case DOCTYPE_YSTEM:
                    doctypeystemloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Otherwise, if the six characters starting from the
                         * current input character are an ASCII case-insensitive
                         * match for the word "SYSTEM", then consume those
                         * characters and switch to the before DOCTYPE system
                         * identifier state.
                         */
                        if (index < 5) { // YSTEM.length
                            char folded = c;
                            if (c >= 'A' && c <= 'Z') {
                                folded += 0x20;
                            }
                            if (folded != Tokenizer.YSTEM[index]) {
                                bogusDoctype();
                                state = Tokenizer.BOGUS_DOCTYPE;
                                reconsume = true;
                                continue stateloop;
                            }
                            index++;
                            continue stateloop;
                        } else {
                            state = Tokenizer.AFTER_DOCTYPE_SYSTEM_KEYWORD;
                            reconsume = true;
                            break doctypeystemloop;
                            // continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case AFTER_DOCTYPE_SYSTEM_KEYWORD:
                    afterdoctypesystemkeywordloop: for (;;) {
                        if (reconsume) {
                            reconsume = false;
                        } else {
                            if (++pos == endPos) {
                                break stateloop;
                            }
                            c = checkChar(buf, pos);
                        }
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                state = Tokenizer.BEFORE_DOCTYPE_SYSTEM_IDENTIFIER;
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE
                                 * Switch to the before DOCTYPE public
                                 * identifier state.
                                 */
                                state = Tokenizer.BEFORE_DOCTYPE_SYSTEM_IDENTIFIER;
                                break afterdoctypesystemkeywordloop;
                            // FALL THROUGH continue stateloop
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Parse Error.
                                 */
                                errNoSpaceBetweenDoctypeSystemKeywordAndQuote();
                                /*
                                 * Set the DOCTYPE token's system identifier to
                                 * the empty string (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE public identifier
                                 * (double-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
                                continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Parse Error.
                                 */
                                errNoSpaceBetweenDoctypeSystemKeywordAndQuote();
                                /*
                                 * Set the DOCTYPE token's public identifier to
                                 * the empty string (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE public identifier
                                 * (single-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
                                continue stateloop;
                            case '>':
                                /* U+003E GREATER-THAN SIGN (>) Parse error. */
                                errExpectedPublicId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            default:
                                bogusDoctype();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
                    beforedoctypesystemidentifierloop: for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\r':
                                silentCarriageReturn();
                                break stateloop;
                            case '\n':
                                silentLineFeed();
                                // fall thru
                            case ' ':
                            case '\t':
                            case '\u000C':
                                /*
                                 * U+0009 CHARACTER TABULATION U+000A LINE FEED
                                 * (LF) U+000C FORM FEED (FF) U+0020 SPACE Stay
                                 * in the before DOCTYPE system identifier
                                 * state.
                                 */
                                continue;
                            case '"':
                                /*
                                 * U+0022 QUOTATION MARK (") Set the DOCTYPE
                                 * token's system identifier to the empty string
                                 * (not missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE system identifier
                                 * (double-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
                                continue stateloop;
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Set the DOCTYPE token's
                                 * system identifier to the empty string (not
                                 * missing),
                                 */
                                clearLongStrBuf();
                                /*
                                 * then switch to the DOCTYPE system identifier
                                 * (single-quoted) state.
                                 */
                                state = Tokenizer.DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
                                break beforedoctypesystemidentifierloop;
                            // continue stateloop;
                            case '>':
                                /* U+003E GREATER-THAN SIGN (>) Parse error. */
                                errExpectedSystemId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            default:
                                bogusDoctype();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                // done by bogusDoctype();
                                /*
                                 * Switch to the bogus DOCTYPE state.
                                 */
                                state = Tokenizer.BOGUS_DOCTYPE;
                                continue stateloop;
                        }
                    }
                    // FALLTHRU DON'T REORDER
                case DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Switch to the after
                                 * DOCTYPE system identifier state.
                                 */
                                systemIdentifier = longStrBufToString();
                                state = Tokenizer.AFTER_DOCTYPE_SYSTEM_IDENTIFIER;
                                continue stateloop;
                            case '>':
                                errGtInSystemId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                systemIdentifier = longStrBufToString();
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the current input
                                 * character to the current DOCTYPE token's
                                 * system identifier.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the DOCTYPE system identifier
                                 * (double-quoted) state.
                                 */
                                continue;
                        }
                    }
                    // XXX reorder point
                case DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
                    for (;;) {
                        if (++pos == endPos) {
                            break stateloop;
                        }
                        c = checkChar(buf, pos);
                        /*
                         * Consume the next input character:
                         */
                        switch (c) {
                            case '\'':
                                /*
                                 * U+0027 APOSTROPHE (') Switch to the after
                                 * DOCTYPE public identifier state.
                                 */
                                publicIdentifier = longStrBufToString();
                                state = Tokenizer.AFTER_DOCTYPE_PUBLIC_IDENTIFIER;
                                continue stateloop;
                            case '>':
                                errGtInPublicId();
                                /*
                                 * Set the DOCTYPE token's force-quirks flag to
                                 * on.
                                 */
                                forceQuirks = true;
                                /*
                                 * Emit that DOCTYPE token.
                                 */
                                publicIdentifier = longStrBufToString();
                                emitDoctypeToken(pos);
                                /*
                                 * Switch to the data state.
                                 */
                                state = Tokenizer.DATA;
                                continue stateloop;
                            case '\r':
                                appendLongStrBufCarriageReturn();
                                break stateloop;
                            case '\n':
                                appendLongStrBufLineFeed();
                                continue;
                            case '\u0000':
                                c = '\uFFFD';
                                // fall thru
                            default:
                                /*
                                 * Anything else Append the current input
                                 * character to the current DOCTYPE token's
                                 * public identifier.
                                 */
                                appendLongStrBuf(c);
                                /*
                                 * Stay in the DOCTYPE public identifier
                                 * (single-quoted) state.
                                 */
                                continue;
                        }
                    }
                    // END HOTSPOT WORKAROUND
            }
        }
        flushChars(buf, pos);
        /*
         * if (prevCR && pos != endPos) { // why is this needed? pos--; col--; }
         */
        // Save locals
        stateSave = state;
        returnStateSave = returnState;
        return pos;
    }
    
    // HOTSPOT WORKAROUND INSERTION POINT
    
    private void initDoctypeFields() {
        Portability.releaseLocal(doctypeName);
        doctypeName = "";
        if (systemIdentifier != null) {
            Portability.releaseString(systemIdentifier);
            systemIdentifier = null;
        }
        if (publicIdentifier != null) {
            Portability.releaseString(publicIdentifier);
            publicIdentifier = null;
        }
        forceQuirks = false;
    }

    @Inline private void adjustDoubleHyphenAndAppendToLongStrBufCarriageReturn()
            throws SAXException {
        silentCarriageReturn();
        adjustDoubleHyphenAndAppendToLongStrBufAndErr('\n');
    }

    @Inline private void adjustDoubleHyphenAndAppendToLongStrBufLineFeed()
            throws SAXException {
        silentLineFeed();
        adjustDoubleHyphenAndAppendToLongStrBufAndErr('\n');
    }

    @Inline private void appendLongStrBufLineFeed() {
        silentLineFeed();
        appendLongStrBuf('\n');
    }

    @Inline private void appendLongStrBufCarriageReturn() {
        silentCarriageReturn();
        appendLongStrBuf('\n');
    }

    @Inline protected void silentCarriageReturn() {
        ++line;
        lastCR = true;
    }

    @Inline protected void silentLineFeed() {
        ++line;
    }

    private void emitCarriageReturn(@NoLength char[] buf, int pos)
            throws SAXException {
        silentCarriageReturn();
        flushChars(buf, pos);
        tokenHandler.characters(Tokenizer.LF, 0, 1);
        cstart = Integer.MAX_VALUE;
    }

    private void emitReplacementCharacter(@NoLength char[] buf, int pos)
            throws SAXException {
        flushChars(buf, pos);
        tokenHandler.zeroOriginatingReplacementCharacter();
        cstart = pos + 1;
    }

    private void setAdditionalAndRememberAmpersandLocation(char add) {
        additional = add;
        // [NOCPP[
        ampersandLocation = new LocatorImpl(this);
        // ]NOCPP]
    }

    private void bogusDoctype() throws SAXException {
        errBogusDoctype();
        forceQuirks = true;
    }

    private void bogusDoctypeWithoutQuirks() throws SAXException {
        errBogusDoctype();
        forceQuirks = false;
    }

    private void emitOrAppendStrBuf(int returnState) throws SAXException {
        if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
            appendStrBufToLongStrBuf();
        } else {
            emitStrBuf();
        }
    }

    private void handleNcrValue(int returnState) throws SAXException {
        /*
         * If one or more characters match the range, then take them all and
         * interpret the string of characters as a number (either hexadecimal or
         * decimal as appropriate).
         */
        if (value <= 0xFFFF) {
            if (value >= 0x80 && value <= 0x9f) {
                /*
                 * If that number is one of the numbers in the first column of
                 * the following table, then this is a parse error.
                 */
                errNcrInC1Range();
                /*
                 * Find the row with that number in the first column, and return
                 * a character token for the Unicode character given in the
                 * second column of that row.
                 */
                @NoLength char[] val = NamedCharacters.WINDOWS_1252[value - 0x80];
                emitOrAppendOne(val, returnState);
                // [NOCPP[
            } else if (value == 0xC
                    && contentSpacePolicy != XmlViolationPolicy.ALLOW) {
                if (contentSpacePolicy == XmlViolationPolicy.ALTER_INFOSET) {
                    emitOrAppendOne(Tokenizer.SPACE, returnState);
                } else if (contentSpacePolicy == XmlViolationPolicy.FATAL) {
                    fatal("A character reference expanded to a form feed which is not legal XML 1.0 white space.");
                }
                // ]NOCPP]
            } else if (value == 0x0) {
                errNcrZero();
                emitOrAppendOne(Tokenizer.REPLACEMENT_CHARACTER, returnState);
            } else if ((value & 0xF800) == 0xD800) {
                errNcrSurrogate();
                emitOrAppendOne(Tokenizer.REPLACEMENT_CHARACTER, returnState);
            } else {
                /*
                 * Otherwise, return a character token for the Unicode character
                 * whose code point is that number.
                 */
                char ch = (char) value;
                // [NOCPP[
                if (value == 0x0D) {
                    errNcrCr();
                } else if ((value <= 0x0008) || (value == 0x000B)
                        || (value >= 0x000E && value <= 0x001F)) {
                    ch = errNcrControlChar(ch);
                } else if (value >= 0xFDD0 && value <= 0xFDEF) {
                    errNcrUnassigned();
                } else if ((value & 0xFFFE) == 0xFFFE) {
                    ch = errNcrNonCharacter(ch);
                } else if (value >= 0x007F && value <= 0x009F) {
                    errNcrControlChar();
                } else {
                    maybeWarnPrivateUse(ch);
                }
                // ]NOCPP]
                bmpChar[0] = ch;
                emitOrAppendOne(bmpChar, returnState);
            }
        } else if (value <= 0x10FFFF) {
            // [NOCPP[
            maybeWarnPrivateUseAstral();
            if ((value & 0xFFFE) == 0xFFFE) {
                errAstralNonCharacter(value);
            }
            // ]NOCPP]
            astralChar[0] = (char) (Tokenizer.LEAD_OFFSET + (value >> 10));
            astralChar[1] = (char) (0xDC00 + (value & 0x3FF));
            emitOrAppendTwo(astralChar, returnState);
        } else {
            errNcrOutOfRange();
            emitOrAppendOne(Tokenizer.REPLACEMENT_CHARACTER, returnState);
        }
    }

    public void eof() throws SAXException {
        int state = stateSave;
        int returnState = returnStateSave;

        eofloop: for (;;) {
            switch (state) {
                case SCRIPT_DATA_LESS_THAN_SIGN:
                case SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN:
                    /*
                     * Otherwise, emit a U+003C LESS-THAN SIGN character token
                     */
                    tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                    /*
                     * and reconsume the current input character in the data
                     * state.
                     */
                    break eofloop;
                case TAG_OPEN:
                    /*
                     * The behavior of this state depends on the content model
                     * flag.
                     */
                    /*
                     * Anything else Parse error.
                     */
                    errEofAfterLt();
                    /*
                     * Emit a U+003C LESS-THAN SIGN character token
                     */
                    tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                    /*
                     * and reconsume the current input character in the data
                     * state.
                     */
                    break eofloop;
                case RAWTEXT_RCDATA_LESS_THAN_SIGN:
                    /*
                     * Emit a U+003C LESS-THAN SIGN character token
                     */
                    tokenHandler.characters(Tokenizer.LT_GT, 0, 1);
                    /*
                     * and reconsume the current input character in the RCDATA
                     * state.
                     */
                    break eofloop;
                case NON_DATA_END_TAG_NAME:
                    /*
                     * Emit a U+003C LESS-THAN SIGN character token, a U+002F
                     * SOLIDUS character token,
                     */
                    tokenHandler.characters(Tokenizer.LT_SOLIDUS, 0, 2);
                    /*
                     * a character token for each of the characters in the
                     * temporary buffer (in the order they were added to the
                     * buffer),
                     */
                    emitStrBuf();
                    /*
                     * and reconsume the current input character in the RCDATA
                     * state.
                     */
                    break eofloop;
                case CLOSE_TAG_OPEN:
                    /* EOF Parse error. */
                    errEofAfterLt();
                    /*
                     * Emit a U+003C LESS-THAN SIGN character token and a U+002F
                     * SOLIDUS character token.
                     */
                    tokenHandler.characters(Tokenizer.LT_SOLIDUS, 0, 2);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case TAG_NAME:
                    /*
                     * EOF Parse error.
                     */
                    errEofInTagName();
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case BEFORE_ATTRIBUTE_NAME:
                case AFTER_ATTRIBUTE_VALUE_QUOTED:
                case SELF_CLOSING_START_TAG:
                    /* EOF Parse error. */
                    errEofWithoutGt();
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case ATTRIBUTE_NAME:
                    /*
                     * EOF Parse error.
                     */
                    errEofInAttributeName();
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case AFTER_ATTRIBUTE_NAME:
                case BEFORE_ATTRIBUTE_VALUE:
                    /* EOF Parse error. */
                    errEofWithoutGt();
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case ATTRIBUTE_VALUE_DOUBLE_QUOTED:
                case ATTRIBUTE_VALUE_SINGLE_QUOTED:
                case ATTRIBUTE_VALUE_UNQUOTED:
                    /* EOF Parse error. */
                    errEofInAttributeValue();
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case BOGUS_COMMENT:
                    emitComment(0, 0);
                    break eofloop;
                case BOGUS_COMMENT_HYPHEN:
                    // [NOCPP[
                    maybeAppendSpaceToBogusComment();
                    // ]NOCPP]
                    emitComment(0, 0);
                    break eofloop;
                case MARKUP_DECLARATION_OPEN:
                    errBogusComment();
                    clearLongStrBuf();
                    emitComment(0, 0);
                    break eofloop;
                case MARKUP_DECLARATION_HYPHEN:
                    errBogusComment();
                    emitComment(0, 0);
                    break eofloop;
                case MARKUP_DECLARATION_OCTYPE:
                    if (index < 6) {
                        errBogusComment();
                        emitComment(0, 0);
                    } else {
                        /* EOF Parse error. */
                        errEofInDoctype();
                        /*
                         * Create a new DOCTYPE token. Set its force-quirks flag
                         * to on.
                         */
                        Portability.releaseLocal(doctypeName);
                        doctypeName = "";
                        if (systemIdentifier != null) {
                            Portability.releaseString(systemIdentifier);
                            systemIdentifier = null;
                        }
                        if (publicIdentifier != null) {
                            Portability.releaseString(publicIdentifier);
                            publicIdentifier = null;
                        }
                        forceQuirks = true;
                        /*
                         * Emit the token.
                         */
                        emitDoctypeToken(0);
                        /*
                         * Reconsume the EOF character in the data state.
                         */
                        break eofloop;
                    }
                    break eofloop;
                case COMMENT_START:
                case COMMENT:
                case COMMENT_END_SPACE:
                    /*
                     * EOF Parse error.
                     */
                    errEofInComment();
                    /* Emit the comment token. */
                    emitComment(0, 0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case COMMENT_END:
                    errEofInComment();
                    /* Emit the comment token. */
                    emitComment(2, 0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case COMMENT_END_DASH:
                case COMMENT_START_DASH:
                    errEofInComment();
                    /* Emit the comment token. */
                    emitComment(1, 0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case COMMENT_END_BANG:
                    errEofInComment();
                    /* Emit the comment token. */
                    emitComment(3, 0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case DOCTYPE:
                case BEFORE_DOCTYPE_NAME:
                    errEofInDoctype();
                    /*
                     * Create a new DOCTYPE token. Set its force-quirks flag to
                     * on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit the token.
                     */
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case DOCTYPE_NAME:
                    errEofInDoctype();
                    strBufToDoctypeName();
                    /*
                     * Set the DOCTYPE token's force-quirks flag to on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit that DOCTYPE token.
                     */
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case DOCTYPE_UBLIC:
                case DOCTYPE_YSTEM:
                case AFTER_DOCTYPE_NAME:
                case AFTER_DOCTYPE_PUBLIC_KEYWORD:
                case AFTER_DOCTYPE_SYSTEM_KEYWORD:
                case BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
                    errEofInDoctype();
                    /*
                     * Set the DOCTYPE token's force-quirks flag to on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit that DOCTYPE token.
                     */
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
                case DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
                    /* EOF Parse error. */
                    errEofInPublicId();
                    /*
                     * Set the DOCTYPE token's force-quirks flag to on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit that DOCTYPE token.
                     */
                    publicIdentifier = longStrBufToString();
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
                case BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
                case BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
                    errEofInDoctype();
                    /*
                     * Set the DOCTYPE token's force-quirks flag to on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit that DOCTYPE token.
                     */
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
                case DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
                    /* EOF Parse error. */
                    errEofInSystemId();
                    /*
                     * Set the DOCTYPE token's force-quirks flag to on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit that DOCTYPE token.
                     */
                    systemIdentifier = longStrBufToString();
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
                    errEofInDoctype();
                    /*
                     * Set the DOCTYPE token's force-quirks flag to on.
                     */
                    forceQuirks = true;
                    /*
                     * Emit that DOCTYPE token.
                     */
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case BOGUS_DOCTYPE:
                    /*
                     * Emit that DOCTYPE token.
                     */
                    emitDoctypeToken(0);
                    /*
                     * Reconsume the EOF character in the data state.
                     */
                    break eofloop;
                case CONSUME_CHARACTER_REFERENCE:
                    /*
                     * Unlike the definition is the spec, this state does not
                     * return a value and never requires the caller to
                     * backtrack. This state takes care of emitting characters
                     * or appending to the current attribute value. It also
                     * takes care of that in the case when consuming the entity
                     * fails.
                     */
                    /*
                     * This section defines how to consume an entity. This
                     * definition is used when parsing entities in text and in
                     * attributes.
                     * 
                     * The behavior depends on the identity of the next
                     * character (the one immediately after the U+0026 AMPERSAND
                     * character):
                     */

                    emitOrAppendStrBuf(returnState);
                    state = returnState;
                    continue;
                case CHARACTER_REFERENCE_HILO_LOOKUP:
                    errNoNamedCharacterMatch();
                    emitOrAppendStrBuf(returnState);
                    state = returnState;
                    continue;
                case CHARACTER_REFERENCE_TAIL:
                    outer: for (;;) {
                        char c = '\u0000';
                        entCol++;
                        /*
                         * Consume the maximum number of characters possible,
                         * with the consumed characters matching one of the
                         * identifiers in the first column of the named
                         * character references table (in a case-sensitive
                         * manner).
                         */
                        hiloop: for (;;) {
                            if (hi == -1) {
                                break hiloop;
                            }
                            if (entCol == NamedCharacters.NAMES[hi].length) {
                                break hiloop;
                            }
                            if (entCol > NamedCharacters.NAMES[hi].length) {
                                break outer;
                            } else if (c < NamedCharacters.NAMES[hi][entCol]) {
                                hi--;
                            } else {
                                break hiloop;
                            }
                        }

                        loloop: for (;;) {
                            if (hi < lo) {
                                break outer;
                            }
                            if (entCol == NamedCharacters.NAMES[lo].length) {
                                candidate = lo;
                                strBufMark = strBufLen;
                                lo++;
                            } else if (entCol > NamedCharacters.NAMES[lo].length) {
                                break outer;
                            } else if (c > NamedCharacters.NAMES[lo][entCol]) {
                                lo++;
                            } else {
                                break loloop;
                            }
                        }
                        if (hi < lo) {
                            break outer;
                        }
                        continue;
                    }

                    // TODO warn about apos (IE) and TRADE (Opera)
                    if (candidate == -1) {
                        /*
                         * If no match can be made, then this is a parse error.
                         */
                        errNoNamedCharacterMatch();
                        emitOrAppendStrBuf(returnState);
                        state = returnState;
                        continue eofloop;
                    } else {
                        byte[] candidateArr = NamedCharacters.NAMES[candidate];
                        if (candidateArr.length == 0
                                || candidateArr[candidateArr.length - 1] != ';') {
                            /*
                             * If the last character matched is not a U+003B
                             * SEMICOLON (;), there is a parse error.
                             */
                            if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
                                /*
                                 * If the entity is being consumed as part of an
                                 * attribute, and the last character matched is
                                 * not a U+003B SEMICOLON (;),
                                 */
                                char ch;
                                if (strBufMark == strBufLen) {
                                    ch = '\u0000';
                                } else {
                                    ch = strBuf[strBufMark];
                                }
                                if ((ch >= '0' && ch <= '9')
                                        || (ch >= 'A' && ch <= 'Z')
                                        || (ch >= 'a' && ch <= 'z')) {
                                    /*
                                     * and the next character is in the range
                                     * U+0030 DIGIT ZERO to U+0039 DIGIT NINE,
                                     * U+0041 LATIN CAPITAL LETTER A to U+005A
                                     * LATIN CAPITAL LETTER Z, or U+0061 LATIN
                                     * SMALL LETTER A to U+007A LATIN SMALL
                                     * LETTER Z, then, for historical reasons,
                                     * all the characters that were matched
                                     * after the U+0026 AMPERSAND (&) must be
                                     * unconsumed, and nothing is returned.
                                     */
                                    errNoNamedCharacterMatch();
                                    appendStrBufToLongStrBuf();
                                    state = returnState;
                                    continue eofloop;
                                }
                            }
                            if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
                                errUnescapedAmpersandInterpretedAsCharacterReference();
                            } else {
                                errNotSemicolonTerminated();
                            }
                        }

                        /*
                         * Otherwise, return a character token for the character
                         * corresponding to the entity name (as given by the
                         * second column of the named character references
                         * table).
                         */
                        @Const @NoLength char[] val = NamedCharacters.VALUES[candidate];
                        // See if the first slot holds a high surrogate
                        if ((val[0] & 0xFC00) == 0xD800) {
                            emitOrAppendTwo(val, returnState);
                        } else {
                            emitOrAppendOne(val, returnState);
                        }
                        // this is so complicated!
                        if (strBufMark < strBufLen) {
                            if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
                                for (int i = strBufMark; i < strBufLen; i++) {
                                    appendLongStrBuf(strBuf[i]);
                                }
                            } else {
                                tokenHandler.characters(strBuf, strBufMark,
                                        strBufLen - strBufMark);
                            }
                        }
                        state = returnState;
                        continue eofloop;
                        /*
                         * If the markup contains I'm &notit; I tell you, the
                         * entity is parsed as "not", as in, I'm ¬it; I tell
                         * you. But if the markup was I'm &notin; I tell you,
                         * the entity would be parsed as "notin;", resulting in
                         * I'm ∉ I tell you.
                         */
                    }
                case CONSUME_NCR:
                case DECIMAL_NRC_LOOP:
                case HEX_NCR_LOOP:
                    /*
                     * If no characters match the range, then don't consume any
                     * characters (and unconsume the U+0023 NUMBER SIGN
                     * character and, if appropriate, the X character). This is
                     * a parse error; nothing is returned.
                     * 
                     * Otherwise, if the next character is a U+003B SEMICOLON,
                     * consume that too. If it isn't, there is a parse error.
                     */
                    if (!seenDigits) {
                        errNoDigitsInNCR();
                        emitOrAppendStrBuf(returnState);
                        state = returnState;
                        continue;
                    } else {
                        errCharRefLacksSemicolon();
                    }
                    // WARNING previous state sets reconsume
                    handleNcrValue(returnState);
                    state = returnState;
                    continue;
                case DATA:
                default:
                    break eofloop;
            }
        }
        // case DATA:
        /*
         * EOF Emit an end-of-file token.
         */
        tokenHandler.eof();
        return;
    }

    private void emitDoctypeToken(int pos) throws SAXException {
        cstart = pos + 1;
        tokenHandler.doctype(doctypeName, publicIdentifier, systemIdentifier,
                forceQuirks);
        // It is OK and sufficient to release these here, since
        // there's no way out of the doctype states than through paths
        // that call this method.
        Portability.releaseLocal(doctypeName);
        doctypeName = null;
        Portability.releaseString(publicIdentifier);
        publicIdentifier = null;
        Portability.releaseString(systemIdentifier);
        systemIdentifier = null;
    }

    @Inline protected char checkChar(@NoLength char[] buf, int pos)
            throws SAXException {
        return buf[pos];
    }

    // [NOCPP[

    /**
     * Returns the alreadyComplainedAboutNonAscii.
     * 
     * @return the alreadyComplainedAboutNonAscii
     */
    public boolean isAlreadyComplainedAboutNonAscii() {
        return true;
    }

    // ]NOCPP]

    public void internalEncodingDeclaration(String internalCharset)
            throws SAXException {
        if (encodingDeclarationHandler != null) {
            encodingDeclarationHandler.internalEncodingDeclaration(internalCharset);
        }
    }

    /**
     * @param val
     * @throws SAXException
     */
    private void emitOrAppendTwo(@Const @NoLength char[] val, int returnState)
            throws SAXException {
        if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
            appendLongStrBuf(val[0]);
            appendLongStrBuf(val[1]);
        } else {
            tokenHandler.characters(val, 0, 2);
        }
    }

    private void emitOrAppendOne(@Const @NoLength char[] val, int returnState)
            throws SAXException {
        if ((returnState & DATA_AND_RCDATA_MASK) != 0) {
            appendLongStrBuf(val[0]);
        } else {
            tokenHandler.characters(val, 0, 1);
        }
    }

    public void end() throws SAXException {
        Portability.releaseArray(strBuf);
        strBuf = null;
        Portability.releaseArray(longStrBuf);
        longStrBuf = null;
        Portability.releaseLocal(doctypeName);
        doctypeName = null;
        if (systemIdentifier != null) {
            Portability.releaseString(systemIdentifier);
            systemIdentifier = null;
        }
        if (publicIdentifier != null) {
            Portability.releaseString(publicIdentifier);
            publicIdentifier = null;
        }
        if (tagName != null) {
            tagName.release();
            tagName = null;
        }
        if (attributeName != null) {
            attributeName.release();
            attributeName = null;
        }
        tokenHandler.endTokenization();
        if (attributes != null) {
            attributes.clear(mappingLangToXmlLang);
            Portability.delete(attributes);
            attributes = null;
        }
    }

    public void requestSuspension() {
        shouldSuspend = true;
    }

    // [NOCPP[
    
    public void becomeConfident() {
        confident = true;
    }

    /**
     * Returns the nextCharOnNewLine.
     * 
     * @return the nextCharOnNewLine
     */
    public boolean isNextCharOnNewLine() {
        return false;
    }

    public boolean isPrevCR() {
        return lastCR;
    }

    /**
     * Returns the line.
     * 
     * @return the line
     */
    public int getLine() {
        return -1;
    }

    /**
     * Returns the col.
     * 
     * @return the col
     */
    public int getCol() {
        return -1;
    }

    // ]NOCPP]
    
    public boolean isInDataState() {
        return (stateSave == DATA);
    }

    public void resetToDataState() {
        strBufLen = 0;
        longStrBufLen = 0;
        stateSave = Tokenizer.DATA;
        // line = 1; XXX line numbers
        lastCR = false;
        index = 0;
        forceQuirks = false;
        additional = '\u0000';
        entCol = -1;
        firstCharKey = -1;
        lo = 0;
        hi = (NamedCharacters.NAMES.length - 1);
        candidate = -1;
        strBufMark = 0;
        prevValue = -1;
        value = 0;
        seenDigits = false;
        endTag = false;
        shouldSuspend = false;
        initDoctypeFields();
        if (tagName != null) {
            tagName.release();
            tagName = null;
        }
        if (attributeName != null) {
            attributeName.release();
            attributeName = null;
        }
        // [NOCPP[
        if (newAttributesEachTime) {
            // ]NOCPP]
            if (attributes != null) {
                Portability.delete(attributes);
                attributes = null;
            }
            // [NOCPP[
        }
        // ]NOCPP]
    }

    public void loadState(Tokenizer other) throws SAXException {
        strBufLen = other.strBufLen;
        if (strBufLen > strBuf.length) {
            Portability.releaseArray(strBuf);
            strBuf = new char[strBufLen];
        }
        System.arraycopy(other.strBuf, 0, strBuf, 0, strBufLen);

        longStrBufLen = other.longStrBufLen;
        if (longStrBufLen > longStrBuf.length) {
            Portability.releaseArray(longStrBuf);
            longStrBuf = new char[longStrBufLen];
        }
        System.arraycopy(other.longStrBuf, 0, longStrBuf, 0, longStrBufLen);

        stateSave = other.stateSave;
        returnStateSave = other.returnStateSave;
        endTagExpectation = other.endTagExpectation;
        endTagExpectationAsArray = other.endTagExpectationAsArray;
        // line = 1; XXX line numbers
        lastCR = other.lastCR;
        index = other.index;
        forceQuirks = other.forceQuirks;
        additional = other.additional;
        entCol = other.entCol;
        firstCharKey = other.firstCharKey;
        lo = other.lo;
        hi = other.hi;
        candidate = other.candidate;
        strBufMark = other.strBufMark;
        prevValue = other.prevValue;
        value = other.value;
        seenDigits = other.seenDigits;
        endTag = other.endTag;
        shouldSuspend = false;

        Portability.releaseLocal(doctypeName);
        if (other.doctypeName == null) {
            doctypeName = null;
        } else {
            doctypeName = Portability.newLocalFromLocal(other.doctypeName,
                    interner);
        }

        Portability.releaseString(systemIdentifier);
        if (other.systemIdentifier == null) {
            systemIdentifier = null;
        } else {
            systemIdentifier = Portability.newStringFromString(other.systemIdentifier);
        }

        Portability.releaseString(publicIdentifier);
        if (other.publicIdentifier == null) {
            publicIdentifier = null;
        } else {
            publicIdentifier = Portability.newStringFromString(other.publicIdentifier);
        }

        if (tagName != null) {
            tagName.release();
        }
        if (other.tagName == null) {
            tagName = null;
        } else {
            tagName = other.tagName.cloneElementName(interner);
        }

        if (attributeName != null) {
            attributeName.release();
        }
        if (other.attributeName == null) {
            attributeName = null;
        } else {
            attributeName = other.attributeName.cloneAttributeName(interner);
        }

        if (attributes != null) {
            Portability.delete(attributes);
        }
        if (other.attributes == null) {
            attributes = null;
        } else {
            attributes = other.attributes.cloneAttributes(interner);
        }
    }

    public void initializeWithoutStarting() throws SAXException {
        confident = false;
        strBuf = new char[64];
        longStrBuf = new char[1024];
        line = 1;
        // [NOCPP[
        html4 = false;
        metaBoundaryPassed = false;
        wantsComments = tokenHandler.wantsComments();
        if (!newAttributesEachTime) {
            attributes = new HtmlAttributes(mappingLangToXmlLang);
        }
        // ]NOCPP]
        resetToDataState();
    }

    protected void errGarbageAfterLtSlash() throws SAXException {
    }

    protected void errLtSlashGt() throws SAXException {
    }

    protected void errWarnLtSlashInRcdata() throws SAXException {
    }

    protected void errHtml4LtSlashInRcdata(char folded) throws SAXException {
    }

    protected void errCharRefLacksSemicolon() throws SAXException {
    }

    protected void errNoDigitsInNCR() throws SAXException {
    }

    protected void errGtInSystemId() throws SAXException {
    }

    protected void errGtInPublicId() throws SAXException {
    }

    protected void errNamelessDoctype() throws SAXException {
    }

    protected void errConsecutiveHyphens() throws SAXException {
    }

    protected void errPrematureEndOfComment() throws SAXException {
    }

    protected void errBogusComment() throws SAXException {
    }

    protected void errUnquotedAttributeValOrNull(char c) throws SAXException {
    }

    protected void errSlashNotFollowedByGt() throws SAXException {
    }

    protected void errHtml4XmlVoidSyntax() throws SAXException {
    }

    protected void errNoSpaceBetweenAttributes() throws SAXException {
    }

    protected void errHtml4NonNameInUnquotedAttribute(char c)
            throws SAXException {
    }

    protected void errLtOrEqualsOrGraveInUnquotedAttributeOrNull(char c)
            throws SAXException {
    }

    protected void errAttributeValueMissing() throws SAXException {
    }

    protected void errBadCharBeforeAttributeNameOrNull(char c)
            throws SAXException {
    }

    protected void errEqualsSignBeforeAttributeName() throws SAXException {
    }

    protected void errBadCharAfterLt(char c) throws SAXException {
    }

    protected void errLtGt() throws SAXException {
    }

    protected void errProcessingInstruction() throws SAXException {
    }

    protected void errUnescapedAmpersandInterpretedAsCharacterReference()
            throws SAXException {
    }

    protected void errNotSemicolonTerminated() throws SAXException {
    }

    protected void errNoNamedCharacterMatch() throws SAXException {
    }

    protected void errQuoteBeforeAttributeName(char c) throws SAXException {
    }

    protected void errQuoteOrLtInAttributeNameOrNull(char c)
            throws SAXException {
    }

    protected void errExpectedPublicId() throws SAXException {
    }

    protected void errBogusDoctype() throws SAXException {
    }

    protected void maybeWarnPrivateUseAstral() throws SAXException {
    }

    protected void maybeWarnPrivateUse(char ch) throws SAXException {
    }

    protected void maybeErrAttributesOnEndTag(HtmlAttributes attrs)
            throws SAXException {
    }

    protected void maybeErrSlashInEndTag(boolean selfClosing)
            throws SAXException {
    }

    protected char errNcrNonCharacter(char ch) throws SAXException {
        return ch;
    }

    protected void errAstralNonCharacter(int ch) throws SAXException {
    }

    protected void errNcrSurrogate() throws SAXException {
    }

    protected char errNcrControlChar(char ch) throws SAXException {
        return ch;
    }

    protected void errNcrCr() throws SAXException {
    }

    protected void errNcrInC1Range() throws SAXException {
    }

    protected void errEofInPublicId() throws SAXException {
    }

    protected void errEofInComment() throws SAXException {
    }

    protected void errEofInDoctype() throws SAXException {
    }

    protected void errEofInAttributeValue() throws SAXException {
    }

    protected void errEofInAttributeName() throws SAXException {
    }

    protected void errEofWithoutGt() throws SAXException {
    }

    protected void errEofInTagName() throws SAXException {
    }

    protected void errEofInEndTag() throws SAXException {
    }

    protected void errEofAfterLt() throws SAXException {
    }

    protected void errNcrOutOfRange() throws SAXException {
    }

    protected void errNcrUnassigned() throws SAXException {
    }

    protected void errDuplicateAttribute() throws SAXException {
    }

    protected void errEofInSystemId() throws SAXException {
    }

    protected void errExpectedSystemId() throws SAXException {
    }

    protected void errMissingSpaceBeforeDoctypeName() throws SAXException {
    }

    protected void errHyphenHyphenBang() throws SAXException {
    }

    protected void errNcrControlChar() throws SAXException {
    }

    protected void errNcrZero() throws SAXException {
    }

    protected void errNoSpaceBetweenDoctypeSystemKeywordAndQuote()
            throws SAXException {
    }

    protected void errNoSpaceBetweenPublicAndSystemIds() throws SAXException {
    }

    protected void errNoSpaceBetweenDoctypePublicKeywordAndQuote()
            throws SAXException {
    }

    protected void noteAttributeWithoutValue() throws SAXException {
    }

    protected void noteUnquotedAttributeValue() throws SAXException {
    }

    /**
     * Sets the encodingDeclarationHandler.
     * 
     * @param encodingDeclarationHandler
     *            the encodingDeclarationHandler to set
     */
    public void setEncodingDeclarationHandler(
            EncodingDeclarationHandler encodingDeclarationHandler) {
        this.encodingDeclarationHandler = encodingDeclarationHandler;
    }

}
