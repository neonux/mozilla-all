/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2010 Mozilla Foundation
 * Portions of comments Copyright 2004-2008 Apple Computer, Inc., Mozilla 
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
 * comment are quotes from the WHATWG HTML 5 spec as of 27 June 2007 
 * amended as of June 28 2007.
 * That document came with this statement:
 * "© Copyright 2004-2007 Apple Computer, Inc., Mozilla Foundation, and 
 * Opera Software ASA. You are granted a license to use, reproduce and 
 * create derivative works of this document."
 */

package nu.validator.htmlparser.impl;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import nu.validator.htmlparser.annotation.Const;
import nu.validator.htmlparser.annotation.IdType;
import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.annotation.Literal;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NoLength;
import nu.validator.htmlparser.annotation.NsUri;
import nu.validator.htmlparser.common.DoctypeExpectation;
import nu.validator.htmlparser.common.DocumentMode;
import nu.validator.htmlparser.common.DocumentModeHandler;
import nu.validator.htmlparser.common.Interner;
import nu.validator.htmlparser.common.TokenHandler;
import nu.validator.htmlparser.common.XmlViolationPolicy;

import org.xml.sax.ErrorHandler;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

public abstract class TreeBuilder<T> implements TokenHandler,
        TreeBuilderState<T> {

    // Start dispatch groups

    final static int OTHER = 0;

    final static int A = 1;

    final static int BASE = 2;

    final static int BODY = 3;

    final static int BR = 4;

    final static int BUTTON = 5;

    final static int CAPTION = 6;

    final static int COL = 7;

    final static int COLGROUP = 8;

    final static int FORM = 9;

    final static int FRAME = 10;

    final static int FRAMESET = 11;

    final static int IMAGE = 12;

    final static int INPUT = 13;

    final static int ISINDEX = 14;

    final static int LI = 15;

    final static int LINK = 16;

    final static int MATH = 17;

    final static int META = 18;

    final static int SVG = 19;

    final static int HEAD = 20;

    final static int HR = 22;

    final static int HTML = 23;

    final static int NOBR = 24;

    final static int NOFRAMES = 25;

    final static int NOSCRIPT = 26;

    final static int OPTGROUP = 27;

    final static int OPTION = 28;

    final static int P = 29;

    final static int PLAINTEXT = 30;

    final static int SCRIPT = 31;

    final static int SELECT = 32;

    final static int STYLE = 33;

    final static int TABLE = 34;

    final static int TEXTAREA = 35;

    final static int TITLE = 36;

    final static int TR = 37;

    final static int XMP = 38;

    final static int TBODY_OR_THEAD_OR_TFOOT = 39;

    final static int TD_OR_TH = 40;

    final static int DD_OR_DT = 41;

    final static int H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 = 42;

    final static int MARQUEE_OR_APPLET = 43;

    final static int PRE_OR_LISTING = 44;

    final static int B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U = 45;

    final static int UL_OR_OL_OR_DL = 46;

    final static int IFRAME = 47;

    final static int EMBED_OR_IMG = 48;

    final static int AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR = 49;

    final static int DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU = 50;

    final static int ADDRESS_OR_DIR_OR_ARTICLE_OR_ASIDE_OR_DATAGRID_OR_DETAILS_OR_HGROUP_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_NAV_OR_SECTION = 51;

    final static int RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR = 52;

    final static int RT_OR_RP = 53;

    final static int COMMAND = 54;

    final static int PARAM_OR_SOURCE = 55;

    final static int MGLYPH_OR_MALIGNMARK = 56;

    final static int MI_MO_MN_MS_MTEXT = 57;

    final static int ANNOTATION_XML = 58;

    final static int FOREIGNOBJECT_OR_DESC = 59;

    final static int NOEMBED = 60;

    final static int FIELDSET = 61;

    final static int OUTPUT_OR_LABEL = 62;

    final static int OBJECT = 63;

    final static int FONT = 64;

    final static int KEYGEN = 65;

    // start insertion modes

    private static final int INITIAL = 0;

    private static final int BEFORE_HTML = 1;

    private static final int BEFORE_HEAD = 2;

    private static final int IN_HEAD = 3;

    private static final int IN_HEAD_NOSCRIPT = 4;

    private static final int AFTER_HEAD = 5;

    private static final int IN_BODY = 6;

    private static final int IN_TABLE = 7;

    private static final int IN_CAPTION = 8;

    private static final int IN_COLUMN_GROUP = 9;

    private static final int IN_TABLE_BODY = 10;

    private static final int IN_ROW = 11;

    private static final int IN_CELL = 12;

    private static final int IN_SELECT = 13;

    private static final int IN_SELECT_IN_TABLE = 14;

    private static final int AFTER_BODY = 15;

    private static final int IN_FRAMESET = 16;

    private static final int AFTER_FRAMESET = 17;

    private static final int AFTER_AFTER_BODY = 18;

    private static final int AFTER_AFTER_FRAMESET = 19;

    private static final int TEXT = 20;

    private static final int FRAMESET_OK = 21;

    // start charset states

    private static final int CHARSET_INITIAL = 0;

    private static final int CHARSET_C = 1;

    private static final int CHARSET_H = 2;

    private static final int CHARSET_A = 3;

    private static final int CHARSET_R = 4;

    private static final int CHARSET_S = 5;

    private static final int CHARSET_E = 6;

    private static final int CHARSET_T = 7;

    private static final int CHARSET_EQUALS = 8;

    private static final int CHARSET_SINGLE_QUOTED = 9;

    private static final int CHARSET_DOUBLE_QUOTED = 10;

    private static final int CHARSET_UNQUOTED = 11;

    // end pseudo enums

    private final static char[] ISINDEX_PROMPT = Portability.isIndexPrompt();

    // [NOCPP[

    private final static String[] HTML4_PUBLIC_IDS = {
            "-//W3C//DTD HTML 4.0 Frameset//EN",
            "-//W3C//DTD HTML 4.0 Transitional//EN",
            "-//W3C//DTD HTML 4.0//EN", "-//W3C//DTD HTML 4.01 Frameset//EN",
            "-//W3C//DTD HTML 4.01 Transitional//EN",
            "-//W3C//DTD HTML 4.01//EN" };

    // ]NOCPP]

    @Literal private final static String[] QUIRKY_PUBLIC_IDS = {
            "+//silmaril//dtd html pro v0r11 19970101//",
            "-//advasoft ltd//dtd html 3.0 aswedit + extensions//",
            "-//as//dtd html 3.0 aswedit + extensions//",
            "-//ietf//dtd html 2.0 level 1//",
            "-//ietf//dtd html 2.0 level 2//",
            "-//ietf//dtd html 2.0 strict level 1//",
            "-//ietf//dtd html 2.0 strict level 2//",
            "-//ietf//dtd html 2.0 strict//",
            "-//ietf//dtd html 2.0//",
            "-//ietf//dtd html 2.1e//",
            "-//ietf//dtd html 3.0//",
            "-//ietf//dtd html 3.2 final//",
            "-//ietf//dtd html 3.2//",
            "-//ietf//dtd html 3//",
            "-//ietf//dtd html level 0//",
            "-//ietf//dtd html level 1//",
            "-//ietf//dtd html level 2//",
            "-//ietf//dtd html level 3//",
            "-//ietf//dtd html strict level 0//",
            "-//ietf//dtd html strict level 1//",
            "-//ietf//dtd html strict level 2//",
            "-//ietf//dtd html strict level 3//",
            "-//ietf//dtd html strict//",
            "-//ietf//dtd html//",
            "-//metrius//dtd metrius presentational//",
            "-//microsoft//dtd internet explorer 2.0 html strict//",
            "-//microsoft//dtd internet explorer 2.0 html//",
            "-//microsoft//dtd internet explorer 2.0 tables//",
            "-//microsoft//dtd internet explorer 3.0 html strict//",
            "-//microsoft//dtd internet explorer 3.0 html//",
            "-//microsoft//dtd internet explorer 3.0 tables//",
            "-//netscape comm. corp.//dtd html//",
            "-//netscape comm. corp.//dtd strict html//",
            "-//o'reilly and associates//dtd html 2.0//",
            "-//o'reilly and associates//dtd html extended 1.0//",
            "-//o'reilly and associates//dtd html extended relaxed 1.0//",
            "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//",
            "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//",
            "-//spyglass//dtd html 2.0 extended//",
            "-//sq//dtd html 2.0 hotmetal + extensions//",
            "-//sun microsystems corp.//dtd hotjava html//",
            "-//sun microsystems corp.//dtd hotjava strict html//",
            "-//w3c//dtd html 3 1995-03-24//", "-//w3c//dtd html 3.2 draft//",
            "-//w3c//dtd html 3.2 final//", "-//w3c//dtd html 3.2//",
            "-//w3c//dtd html 3.2s draft//", "-//w3c//dtd html 4.0 frameset//",
            "-//w3c//dtd html 4.0 transitional//",
            "-//w3c//dtd html experimental 19960712//",
            "-//w3c//dtd html experimental 970421//", "-//w3c//dtd w3 html//",
            "-//w3o//dtd w3 html 3.0//", "-//webtechs//dtd mozilla html 2.0//",
            "-//webtechs//dtd mozilla html//" };

    private static final int NOT_FOUND_ON_STACK = Integer.MAX_VALUE;

    private static final int IN_FOREIGN = 0;

    private static final int NOT_IN_FOREIGN = 1;

    // [NOCPP[

    private static final @Local String HTML_LOCAL = "html";

    // ]NOCPP]

    private int mode = INITIAL;

    private int originalMode = INITIAL;
    
    /**
     * Used only when moving back to IN_BODY.
     */
    private boolean framesetOk = true;

    private int foreignFlag = TreeBuilder.NOT_IN_FOREIGN;

    protected Tokenizer tokenizer;

    // [NOCPP[

    protected ErrorHandler errorHandler;

    private DocumentModeHandler documentModeHandler;

    private DoctypeExpectation doctypeExpectation = DoctypeExpectation.HTML;

    // ]NOCPP]

    private boolean scriptingEnabled = false;

    private boolean needToDropLF;

    // [NOCPP[

    private boolean wantingComments;

    // ]NOCPP]

    private boolean fragment;

    private @Local String contextName;

    private @NsUri String contextNamespace;

    private T contextNode;

    private StackNode<T>[] stack;

    private int currentPtr = -1;

    private StackNode<T>[] listOfActiveFormattingElements;

    private int listPtr = -1;

    private T formPointer;

    private T headPointer;

    protected char[] charBuffer;

    protected int charBufferLen = 0;

    private boolean quirks = false;

    // [NOCPP[

    private boolean reportingDoctype = true;

    private XmlViolationPolicy namePolicy = XmlViolationPolicy.ALTER_INFOSET;

    private final Map<String, LocatorImpl> idLocations = new HashMap<String, LocatorImpl>();

    private boolean html4;

    // ]NOCPP]

    protected TreeBuilder() {
        fragment = false;
    }

    /**
     * Reports an condition that would make the infoset incompatible with XML
     * 1.0 as fatal.
     * 
     * @throws SAXException
     * @throws SAXParseException
     */
    protected void fatal() throws SAXException {
    }

    // [NOCPP[

    protected final void fatal(Exception e) throws SAXException {
        SAXParseException spe = new SAXParseException(e.getMessage(),
                tokenizer, e);
        if (errorHandler != null) {
            errorHandler.fatalError(spe);
        }
        throw spe;
    }

    final void fatal(String s) throws SAXException {
        SAXParseException spe = new SAXParseException(s, tokenizer);
        if (errorHandler != null) {
            errorHandler.fatalError(spe);
        }
        throw spe;
    }

    // ]NOCPP]

    /**
     * Reports a Parse Error.
     * 
     * @param message
     *            the message
     * @throws SAXException
     */
    final void err(String message) throws SAXException {
        // [NOCPP[
        if (errorHandler == null) {
            return;
        }
        SAXParseException spe = new SAXParseException(message, tokenizer);
        errorHandler.error(spe);
        // ]NOCPP]
    }

    /**
     * Reports a warning
     * 
     * @param message
     *            the message
     * @throws SAXException
     */
    final void warn(String message) throws SAXException {
        // [NOCPP[
        if (errorHandler == null) {
            return;
        }
        SAXParseException spe = new SAXParseException(message, tokenizer);
        errorHandler.warning(spe);
        // ]NOCPP]
    }

    @SuppressWarnings("unchecked") public final void startTokenization(Tokenizer self) throws SAXException {
        tokenizer = self;
        stack = new StackNode[64];
        listOfActiveFormattingElements = new StackNode[64];
        needToDropLF = false;
        originalMode = INITIAL;
        currentPtr = -1;
        listPtr = -1;
        Portability.releaseElement(formPointer);
        formPointer = null;
        Portability.releaseElement(headPointer);
        headPointer = null;
        // [NOCPP[
        html4 = false;
        idLocations.clear();
        wantingComments = wantsComments();
        // ]NOCPP]
        start(fragment);
        charBufferLen = 0;
        charBuffer = new char[1024];
        framesetOk = true;
        if (fragment) {
            T elt;
            if (contextNode != null) {
                elt = contextNode;
                Portability.retainElement(elt);
            } else {
                elt = createHtmlElementSetAsRoot(tokenizer.emptyAttributes());
            }
            StackNode<T> node = new StackNode<T>(
                    "http://www.w3.org/1999/xhtml", ElementName.HTML, elt);
            currentPtr++;
            stack[currentPtr] = node;
            resetTheInsertionMode();
            if ("title" == contextName || "textarea" == contextName) {
                tokenizer.setContentModelFlag(Tokenizer.RCDATA, contextName);
            } else if ("style" == contextName || "xmp" == contextName
                    || "iframe" == contextName || "noembed" == contextName
                    || "noframes" == contextName
                    || (scriptingEnabled && "noscript" == contextName)) {
                tokenizer.setContentModelFlag(Tokenizer.RAWTEXT, contextName);
            } else if ("plaintext" == contextName) {
                tokenizer.setContentModelFlag(Tokenizer.PLAINTEXT, contextName);
            } else if ("script" == contextName) {
                tokenizer.setContentModelFlag(Tokenizer.SCRIPT_DATA,
                        contextName);
            } else {
                tokenizer.setContentModelFlag(Tokenizer.DATA, contextName);
            }
            Portability.releaseLocal(contextName);
            contextName = null;
            Portability.releaseElement(contextNode);
            contextNode = null;
            Portability.releaseElement(elt);
        } else {
            mode = INITIAL;
            foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
        }
    }

    public final void doctype(@Local String name, String publicIdentifier,
            String systemIdentifier, boolean forceQuirks) throws SAXException {
        needToDropLF = false;
        doctypeloop: for (;;) {
            switch (foreignFlag) {
                case IN_FOREIGN:
                    break doctypeloop;
                default:
                    switch (mode) {
                        case INITIAL:
                            // [NOCPP[
                            if (reportingDoctype) {
                                // ]NOCPP]
                                String emptyString = Portability.newEmptyString();
                                appendDoctypeToDocument(name == null ? ""
                                        : name,
                                        publicIdentifier == null ? emptyString
                                                : publicIdentifier,
                                        systemIdentifier == null ? emptyString
                                                : systemIdentifier);
                                Portability.releaseString(emptyString);
                                // [NOCPP[
                            }
                            switch (doctypeExpectation) {
                                case HTML:
                                    // ]NOCPP]
                                    if (isQuirky(name, publicIdentifier,
                                            systemIdentifier, forceQuirks)) {
                                        err("Quirky doctype. Expected \u201C<!DOCTYPE html>\u201D.");
                                        documentModeInternal(
                                                DocumentMode.QUIRKS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, false);
                                    } else if (isAlmostStandards(
                                            publicIdentifier, systemIdentifier)) {
                                        err("Almost standards mode doctype. Expected \u201C<!DOCTYPE html>\u201D.");
                                        documentModeInternal(
                                                DocumentMode.ALMOST_STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, false);
                                    } else {
                                        // [NOCPP[
                                        if ((Portability.literalEqualsString(
                                                "-//W3C//DTD HTML 4.0//EN",
                                                publicIdentifier) && (systemIdentifier == null || Portability.literalEqualsString(
                                                "http://www.w3.org/TR/REC-html40/strict.dtd",
                                                systemIdentifier)))
                                                || (Portability.literalEqualsString(
                                                        "-//W3C//DTD HTML 4.01//EN",
                                                        publicIdentifier) && (systemIdentifier == null || Portability.literalEqualsString(
                                                        "http://www.w3.org/TR/html4/strict.dtd",
                                                        systemIdentifier)))
                                                || (Portability.literalEqualsString(
                                                        "-//W3C//DTD XHTML 1.0 Strict//EN",
                                                        publicIdentifier) && Portability.literalEqualsString(
                                                        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd",
                                                        systemIdentifier))
                                                || (Portability.literalEqualsString(
                                                        "-//W3C//DTD XHTML 1.1//EN",
                                                        publicIdentifier) && Portability.literalEqualsString(
                                                        "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd",
                                                        systemIdentifier))

                                        ) {
                                            warn("Obsolete doctype. Expected \u201C<!DOCTYPE html>\u201D.");
                                        } else if (!((systemIdentifier == null || Portability.literalEqualsString(
                                                "about:legacy-compat",
                                                systemIdentifier)) && publicIdentifier == null)) {
                                            err("Legacy doctype. Expected \u201C<!DOCTYPE html>\u201D.");
                                        }
                                        // ]NOCPP]
                                        documentModeInternal(
                                                DocumentMode.STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, false);
                                    }
                                    // [NOCPP[
                                    break;
                                case HTML401_STRICT:
                                    html4 = true;
                                    tokenizer.turnOnAdditionalHtml4Errors();
                                    if (isQuirky(name, publicIdentifier,
                                            systemIdentifier, forceQuirks)) {
                                        err("Quirky doctype. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                        documentModeInternal(
                                                DocumentMode.QUIRKS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, true);
                                    } else if (isAlmostStandards(
                                            publicIdentifier, systemIdentifier)) {
                                        err("Almost standards mode doctype. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                        documentModeInternal(
                                                DocumentMode.ALMOST_STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, true);
                                    } else {
                                        if ("-//W3C//DTD HTML 4.01//EN".equals(publicIdentifier)) {
                                            if (!"http://www.w3.org/TR/html4/strict.dtd".equals(systemIdentifier)) {
                                                warn("The doctype did not contain the system identifier prescribed by the HTML 4.01 specification. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                            }
                                        } else {
                                            err("The doctype was not the HTML 4.01 Strict doctype. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                        }
                                        documentModeInternal(
                                                DocumentMode.STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, true);
                                    }
                                    break;
                                case HTML401_TRANSITIONAL:
                                    html4 = true;
                                    tokenizer.turnOnAdditionalHtml4Errors();
                                    if (isQuirky(name, publicIdentifier,
                                            systemIdentifier, forceQuirks)) {
                                        err("Quirky doctype. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                        documentModeInternal(
                                                DocumentMode.QUIRKS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, true);
                                    } else if (isAlmostStandards(
                                            publicIdentifier, systemIdentifier)) {
                                        if ("-//W3C//DTD HTML 4.01 Transitional//EN".equals(publicIdentifier)
                                                && systemIdentifier != null) {
                                            if (!"http://www.w3.org/TR/html4/loose.dtd".equals(systemIdentifier)) {
                                                warn("The doctype did not contain the system identifier prescribed by the HTML 4.01 specification. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                            }
                                        } else {
                                            err("The doctype was not a non-quirky HTML 4.01 Transitional doctype. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                        }
                                        documentModeInternal(
                                                DocumentMode.ALMOST_STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, true);
                                    } else {
                                        err("The doctype was not the HTML 4.01 Transitional doctype. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                        documentModeInternal(
                                                DocumentMode.STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, true);
                                    }
                                    break;
                                case AUTO:
                                    html4 = isHtml4Doctype(publicIdentifier);
                                    if (html4) {
                                        tokenizer.turnOnAdditionalHtml4Errors();
                                    }
                                    if (isQuirky(name, publicIdentifier,
                                            systemIdentifier, forceQuirks)) {
                                        err("Quirky doctype. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                                        documentModeInternal(
                                                DocumentMode.QUIRKS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, html4);
                                    } else if (isAlmostStandards(
                                            publicIdentifier, systemIdentifier)) {
                                        if ("-//W3C//DTD HTML 4.01 Transitional//EN".equals(publicIdentifier)) {
                                            if (!"http://www.w3.org/TR/html4/loose.dtd".equals(systemIdentifier)) {
                                                warn("The doctype did not contain the system identifier prescribed by the HTML 4.01 specification. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                            }
                                        } else {
                                            err("Almost standards mode doctype. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                                        }
                                        documentModeInternal(
                                                DocumentMode.ALMOST_STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, html4);
                                    } else {
                                        if ("-//W3C//DTD HTML 4.01//EN".equals(publicIdentifier)) {
                                            if (!"http://www.w3.org/TR/html4/strict.dtd".equals(systemIdentifier)) {
                                                warn("The doctype did not contain the system identifier prescribed by the HTML 4.01 specification. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                            }
                                        } else {
                                            if (!(publicIdentifier == null && systemIdentifier == null)) {
                                                err("Legacy doctype. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                                            }
                                        }
                                        documentModeInternal(
                                                DocumentMode.STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, html4);
                                    }
                                    break;
                                case NO_DOCTYPE_ERRORS:
                                    if (isQuirky(name, publicIdentifier,
                                            systemIdentifier, forceQuirks)) {
                                        documentModeInternal(
                                                DocumentMode.QUIRKS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, false);
                                    } else if (isAlmostStandards(
                                            publicIdentifier, systemIdentifier)) {
                                        documentModeInternal(
                                                DocumentMode.ALMOST_STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, false);
                                    } else {
                                        documentModeInternal(
                                                DocumentMode.STANDARDS_MODE,
                                                publicIdentifier,
                                                systemIdentifier, false);
                                    }
                                    break;
                            }
                            // ]NOCPP]

                            /*
                             * 
                             * Then, switch to the root element mode of the tree
                             * construction stage.
                             */
                            mode = BEFORE_HTML;
                            return;
                        default:
                            break doctypeloop;
                    }
            }

        }
        /*
         * A DOCTYPE token Parse error.
         */
        err("Stray doctype.");
        /*
         * Ignore the token.
         */
        return;
    }

    // [NOCPP[

    private boolean isHtml4Doctype(String publicIdentifier) {
        if (publicIdentifier != null
                && (Arrays.binarySearch(TreeBuilder.HTML4_PUBLIC_IDS,
                        publicIdentifier) > -1)) {
            return true;
        }
        return false;
    }

    // ]NOCPP]

    public final void comment(@NoLength char[] buf, int start, int length)
            throws SAXException {
        needToDropLF = false;
        // [NOCPP[
        if (!wantingComments) {
            return;
        }
        // ]NOCPP]
        commentloop: for (;;) {
            switch (foreignFlag) {
                case IN_FOREIGN:
                    break commentloop;
                default:
                    switch (mode) {
                        case INITIAL:
                        case BEFORE_HTML:
                        case AFTER_AFTER_BODY:
                        case AFTER_AFTER_FRAMESET:
                            /*
                             * A comment token Append a Comment node to the
                             * Document object with the data attribute set to
                             * the data given in the comment token.
                             */
                            appendCommentToDocument(buf, start, length);
                            return;
                        case AFTER_BODY:
                            /*
                             * A comment token Append a Comment node to the
                             * first element in the stack of open elements (the
                             * html element), with the data attribute set to the
                             * data given in the comment token.
                             */
                            flushCharacters();
                            appendComment(stack[0].node, buf, start, length);
                            return;
                        default:
                            break commentloop;
                    }
            }
        }
        /*
         * A comment token Append a Comment node to the current node with the
         * data attribute set to the data given in the comment token.
         */
        flushCharacters();
        appendComment(stack[currentPtr].node, buf, start, length);
        return;
    }

    /**
     * @see nu.validator.htmlparser.common.TokenHandler#characters(char[], int,
     *      int)
     */
    public final void characters(@Const @NoLength char[] buf, int start, int length)
            throws SAXException {
        if (needToDropLF) {
            if (buf[start] == '\n') {
                start++;
                length--;
                if (length == 0) {
                    return;
                }
            }
            needToDropLF = false;
        }

        // optimize the most common case
        // XXX should there be an IN FOREIGN check here?
        switch (mode) {
            case IN_BODY:
            case IN_CELL:
            case IN_CAPTION:
                reconstructTheActiveFormattingElements();
                // fall through
            case TEXT:
                accumulateCharacters(buf, start, length);
                return;
            default:
                int end = start + length;
                charactersloop: for (int i = start; i < end; i++) {
                    switch (buf[i]) {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r':
                        case '\u000C':
                            /*
                             * A character token that is one of one of U+0009
                             * CHARACTER TABULATION, U+000A LINE FEED (LF),
                             * U+000C FORM FEED (FF), or U+0020 SPACE
                             */
                            switch (mode) {
                                case INITIAL:
                                case BEFORE_HTML:
                                case BEFORE_HEAD:
                                    /*
                                     * Ignore the token.
                                     */
                                    start = i + 1;
                                    continue;
                                case FRAMESET_OK:
                                case IN_HEAD:
                                case IN_HEAD_NOSCRIPT:
                                case AFTER_HEAD:
                                case IN_COLUMN_GROUP:
                                case IN_FRAMESET:
                                case AFTER_FRAMESET:
                                    /*
                                     * Append the character to the current node.
                                     */
                                    continue;
                                case IN_BODY:
                                case IN_CELL:
                                case IN_CAPTION:
                                    // XXX is this dead code?
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }

                                    /*
                                     * Reconstruct the active formatting
                                     * elements, if any.
                                     */
                                    reconstructTheActiveFormattingElements();
                                    /*
                                     * Append the token's character to the
                                     * current node.
                                     */
                                    break charactersloop;
                                case IN_SELECT:
                                case IN_SELECT_IN_TABLE:
                                    break charactersloop;
                                case IN_TABLE:
                                case IN_TABLE_BODY:
                                case IN_ROW:
                                    reconstructTheActiveFormattingElements();
                                    accumulateCharacter(buf[i]);
                                    start = i + 1;
                                    continue;
                                case AFTER_BODY:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Reconstruct the active formatting
                                     * elements, if any.
                                     */
                                    reconstructTheActiveFormattingElements();
                                    /*
                                     * Append the token's character to the
                                     * current node.
                                     */
                                    continue;
                                case AFTER_AFTER_BODY:
                                case AFTER_AFTER_FRAMESET:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Reconstruct the active formatting
                                     * elements, if any.
                                     */
                                    reconstructTheActiveFormattingElements();
                                    /*
                                     * Append the token's character to the
                                     * current node.
                                     */
                                    continue;
                            }
                        default:
                            /*
                             * A character token that is not one of one of
                             * U+0009 CHARACTER TABULATION, U+000A LINE FEED
                             * (LF), U+000C FORM FEED (FF), or U+0020 SPACE
                             */
                            switch (mode) {
                                case INITIAL:
                                    /*
                                     * Parse error.
                                     */
                                    // [NOCPP[
                                    switch (doctypeExpectation) {
                                        case AUTO:
                                            err("Non-space characters found without seeing a doctype first. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                                            break;
                                        case HTML:
                                            err("Non-space characters found without seeing a doctype first. Expected \u201C<!DOCTYPE html>\u201D.");
                                            break;
                                        case HTML401_STRICT:
                                            err("Non-space characters found without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                            break;
                                        case HTML401_TRANSITIONAL:
                                            err("Non-space characters found without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                            break;
                                        case NO_DOCTYPE_ERRORS:
                                    }
                                    // ]NOCPP]
                                    /*
                                     * 
                                     * Set the document to quirks mode.
                                     */
                                    documentModeInternal(
                                            DocumentMode.QUIRKS_MODE, null,
                                            null, false);
                                    /*
                                     * Then, switch to the root element mode of
                                     * the tree construction stage
                                     */
                                    mode = BEFORE_HTML;
                                    /*
                                     * and reprocess the current token.
                                     */
                                    i--;
                                    continue;
                                case BEFORE_HTML:
                                    /*
                                     * Create an HTMLElement node with the tag
                                     * name html, in the HTML namespace. Append
                                     * it to the Document object.
                                     */
                                    appendHtmlElementToDocumentAndPush();
                                    /* Switch to the main mode */
                                    mode = BEFORE_HEAD;
                                    /*
                                     * reprocess the current token.
                                     */
                                    i--;
                                    continue;
                                case BEFORE_HEAD:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * /Act as if a start tag token with the tag
                                     * name "head" and no attributes had been
                                     * seen,
                                     */
                                    appendToCurrentNodeAndPushHeadElement(HtmlAttributes.EMPTY_ATTRIBUTES);
                                    mode = IN_HEAD;
                                    /*
                                     * then reprocess the current token.
                                     * 
                                     * This will result in an empty head element
                                     * being generated, with the current token
                                     * being reprocessed in the "after head"
                                     * insertion mode.
                                     */
                                    i--;
                                    continue;
                                case IN_HEAD:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Act as if an end tag token with the tag
                                     * name "head" had been seen,
                                     */
                                    pop();
                                    mode = AFTER_HEAD;
                                    /*
                                     * and reprocess the current token.
                                     */
                                    i--;
                                    continue;
                                case IN_HEAD_NOSCRIPT:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Parse error. Act as if an end tag with
                                     * the tag name "noscript" had been seen
                                     */
                                    err("Non-space character inside \u201Cnoscript\u201D inside \u201Chead\u201D.");
                                    pop();
                                    mode = IN_HEAD;
                                    /*
                                     * and reprocess the current token.
                                     */
                                    i--;
                                    continue;
                                case AFTER_HEAD:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Act as if a start tag token with the tag
                                     * name "body" and no attributes had been
                                     * seen,
                                     */
                                    appendToCurrentNodeAndPushBodyElement();
                                    mode = FRAMESET_OK;
                                    /*
                                     * and then reprocess the current token.
                                     */
                                    i--;
                                    continue;
                                case FRAMESET_OK:
                                    framesetOk = false;
                                    mode = IN_BODY;
                                    i--;
                                    continue;
                                case IN_BODY:
                                case IN_CELL:
                                case IN_CAPTION:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Reconstruct the active formatting
                                     * elements, if any.
                                     */
                                    reconstructTheActiveFormattingElements();
                                    /*
                                     * Append the token's character to the
                                     * current node.
                                     */
                                    break charactersloop;
                                case IN_TABLE:
                                case IN_TABLE_BODY:
                                case IN_ROW:
                                    reconstructTheActiveFormattingElements();
                                    accumulateCharacter(buf[i]);
                                    start = i + 1;
                                    continue;
                                case IN_COLUMN_GROUP:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Act as if an end tag with the tag name
                                     * "colgroup" had been seen, and then, if
                                     * that token wasn't ignored, reprocess the
                                     * current token.
                                     */
                                    if (currentPtr == 0) {
                                        err("Non-space in \u201Ccolgroup\u201D when parsing fragment.");
                                        start = i + 1;
                                        continue;
                                    }
                                    pop();
                                    mode = IN_TABLE;
                                    i--;
                                    continue;
                                case IN_SELECT:
                                case IN_SELECT_IN_TABLE:
                                    break charactersloop;
                                case AFTER_BODY:
                                    err("Non-space character after body.");
                                    fatal();
                                    mode = framesetOk ? FRAMESET_OK : IN_BODY;
                                    i--;
                                    continue;
                                case IN_FRAMESET:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Parse error.
                                     */
                                    err("Non-space in \u201Cframeset\u201D.");
                                    /*
                                     * Ignore the token.
                                     */
                                    start = i + 1;
                                    continue;
                                case AFTER_FRAMESET:
                                    if (start < i) {
                                        accumulateCharacters(buf, start, i
                                                - start);
                                        start = i;
                                    }
                                    /*
                                     * Parse error.
                                     */
                                    err("Non-space after \u201Cframeset\u201D.");
                                    /*
                                     * Ignore the token.
                                     */
                                    start = i + 1;
                                    continue;
                                case AFTER_AFTER_BODY:
                                    /*
                                     * Parse error.
                                     */
                                    err("Non-space character in page trailer.");
                                    /*
                                     * Switch back to the main mode and
                                     * reprocess the token.
                                     */
                                    mode = framesetOk ? FRAMESET_OK : IN_BODY;
                                    i--;
                                    continue;
                                case AFTER_AFTER_FRAMESET:
                                    /*
                                     * Parse error.
                                     */
                                    err("Non-space character in page trailer.");
                                    /*
                                     * Switch back to the main mode and
                                     * reprocess the token.
                                     */
                                    mode = IN_FRAMESET;
                                    i--;
                                    continue;
                            }
                    }
                }
                if (start < end) {
                    accumulateCharacters(buf, start, end - start);
                }
        }
    }

    public final void eof() throws SAXException {
        flushCharacters();
        switch (foreignFlag) {
            case IN_FOREIGN:
                err("End of file in a foreign namespace context.");
                while (stack[currentPtr].ns != "http://www.w3.org/1999/xhtml") {
                    popOnEof();
                }
                foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
            default:
                // fall through
        }
        eofloop: for (;;) {
            switch (mode) {
                case INITIAL:
                    /*
                     * Parse error.
                     */
                    // [NOCPP[
                    switch (doctypeExpectation) {
                        case AUTO:
                            err("End of file seen without seeing a doctype first. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                            break;
                        case HTML:
                            err("End of file seen without seeing a doctype first. Expected \u201C<!DOCTYPE html>\u201D.");
                            break;
                        case HTML401_STRICT:
                            err("End of file seen without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                            break;
                        case HTML401_TRANSITIONAL:
                            err("End of file seen without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                            break;
                        case NO_DOCTYPE_ERRORS:
                    }
                    // ]NOCPP]
                    /*
                     * 
                     * Set the document to quirks mode.
                     */
                    documentModeInternal(DocumentMode.QUIRKS_MODE, null, null,
                            false);
                    /*
                     * Then, switch to the root element mode of the tree
                     * construction stage
                     */
                    mode = BEFORE_HTML;
                    /*
                     * and reprocess the current token.
                     */
                    continue;
                case BEFORE_HTML:
                    /*
                     * Create an HTMLElement node with the tag name html, in the
                     * HTML namespace. Append it to the Document object.
                     */
                    appendHtmlElementToDocumentAndPush();
                    // XXX application cache manifest
                    /* Switch to the main mode */
                    mode = BEFORE_HEAD;
                    /*
                     * reprocess the current token.
                     */
                    continue;
                case BEFORE_HEAD:
                    appendToCurrentNodeAndPushHeadElement(HtmlAttributes.EMPTY_ATTRIBUTES);
                    mode = IN_HEAD;
                    continue;
                case IN_HEAD:
                    if (currentPtr > 1) {
                        err("End of file seen and there were open elements.");
                    }
                    while (currentPtr > 0) {
                        popOnEof();
                    }
                    mode = AFTER_HEAD;
                    continue;
                case IN_HEAD_NOSCRIPT:
                    err("End of file seen and there were open elements.");
                    while (currentPtr > 1) {
                        popOnEof();
                    }
                    mode = IN_HEAD;
                    continue;
                case AFTER_HEAD:
                    appendToCurrentNodeAndPushBodyElement();
                    mode = IN_BODY;
                    continue;
                case IN_COLUMN_GROUP:
                    if (currentPtr == 0) {
                        assert fragment;
                        break eofloop;
                    } else {
                        popOnEof();
                        mode = IN_TABLE;
                        continue;
                    }
                case FRAMESET_OK:
                case IN_CAPTION:
                case IN_CELL:
                case IN_BODY:
                    // [NOCPP[
                    openelementloop: for (int i = currentPtr; i >= 0; i--) {
                        int group = stack[i].group;
                        switch (group) {
                            case DD_OR_DT:
                            case LI:
                            case P:
                            case TBODY_OR_THEAD_OR_TFOOT:
                            case TD_OR_TH:
                            case BODY:
                            case HTML:
                                break;
                            default:
                                err("End of file seen and there were open elements.");
                                break openelementloop;
                        }
                    }
                    // ]NOCPP]
                    break eofloop;
                case TEXT:
                    err("End of file seen when expecting text or an end tag.");
                    // XXX mark script as already executed
                    if (originalMode == AFTER_HEAD) {
                        popOnEof();
                    }
                    popOnEof();
                    mode = originalMode;
                    continue;
                case IN_TABLE_BODY:
                case IN_ROW:
                case IN_TABLE:
                case IN_SELECT:
                case IN_SELECT_IN_TABLE:
                case IN_FRAMESET:
                    if (currentPtr > 0) {
                        err("End of file seen and there were open elements.");
                    }
                    break eofloop;
                case AFTER_BODY:
                case AFTER_FRAMESET:
                case AFTER_AFTER_BODY:
                case AFTER_AFTER_FRAMESET:
                default:
                    // [NOCPP[
                    if (currentPtr == 0) { // This silliness is here to poison
                        // buggy compiler optimizations in
                        // GWT
                        System.currentTimeMillis();
                    }
                    // ]NOCPP]
                    break eofloop;
            }
        }
        while (currentPtr > 0) {
            popOnEof();
        }
        if (!fragment) {
            popOnEof();
        }
        /* Stop parsing. */
    }

    /**
     * @see nu.validator.htmlparser.common.TokenHandler#endTokenization()
     */
    public final void endTokenization() throws SAXException {
        Portability.releaseElement(formPointer);
        formPointer = null;
        Portability.releaseElement(headPointer);
        headPointer = null;
        if (stack != null) {
            while (currentPtr > -1) {
                stack[currentPtr].release();
                currentPtr--;
            }
            Portability.releaseArray(stack);
            stack = null;
        }
        if (listOfActiveFormattingElements != null) {
            while (listPtr > -1) {
                if (listOfActiveFormattingElements[listPtr] != null) {
                    listOfActiveFormattingElements[listPtr].release();
                }
                listPtr--;
            }
            Portability.releaseArray(listOfActiveFormattingElements);
            listOfActiveFormattingElements = null;
        }
        // [NOCPP[
        idLocations.clear();
        // ]NOCPP]
        if (charBuffer != null) {
            Portability.releaseArray(charBuffer);
            charBuffer = null;
        }
        end();
    }

    public final void startTag(ElementName elementName,
            HtmlAttributes attributes, boolean selfClosing) throws SAXException {
        // [NOCPP[
        if (errorHandler != null) {
            // ID uniqueness
            @IdType String id = attributes.getId();
            if (id != null) {
                LocatorImpl oldLoc = idLocations.get(id);
                if (oldLoc != null) {
                    err("Duplicate ID \u201C" + id + "\u201D.");
                    errorHandler.warning(new SAXParseException(
                            "The first occurrence of ID \u201C" + id
                                    + "\u201D was here.", oldLoc));
                } else {
                    idLocations.put(id, new LocatorImpl(tokenizer));
                }
            }
        }
        // ]NOCPP]

        int eltPos;
        needToDropLF = false;
        boolean needsPostProcessing = false;
        starttagloop: for (;;) {
            int group = elementName.group;
            @Local String name = elementName.name;
            switch (foreignFlag) {
                case IN_FOREIGN:
                    StackNode<T> currentNode = stack[currentPtr];
                    @NsUri String currNs = currentNode.ns;
                    int currGroup = currentNode.group;
                    if (("http://www.w3.org/1999/xhtml" == currNs)
                            || ("http://www.w3.org/1998/Math/MathML" == currNs && ((MGLYPH_OR_MALIGNMARK != group && MI_MO_MN_MS_MTEXT == currGroup) || (SVG == group && ANNOTATION_XML == currGroup)))
                            || ("http://www.w3.org/2000/svg" == currNs && (TITLE == currGroup || (FOREIGNOBJECT_OR_DESC == currGroup)))) {
                        needsPostProcessing = true;
                        // fall through to non-foreign behavior
                    } else {
                        switch (group) {
                            case B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
                            case DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
                            case BODY:
                            case BR:
                            case RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR:
                            case DD_OR_DT:
                            case UL_OR_OL_OR_DL:
                            case EMBED_OR_IMG:
                            case H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
                            case HEAD:
                            case HR:
                            case LI:
                            case META:
                            case NOBR:
                            case P:
                            case PRE_OR_LISTING:
                            case TABLE:
                                err("HTML start tag \u201C"
                                        + name
                                        + "\u201D in a foreign namespace context.");
                                while (stack[currentPtr].ns != "http://www.w3.org/1999/xhtml") {
                                    pop();
                                }
                                foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
                                continue starttagloop;
                            case FONT:
                                if (attributes.contains(AttributeName.COLOR)
                                        || attributes.contains(AttributeName.FACE)
                                        || attributes.contains(AttributeName.SIZE)) {
                                    err("HTML start tag \u201C"
                                            + name
                                            + "\u201D in a foreign namespace context.");
                                    while (stack[currentPtr].ns != "http://www.w3.org/1999/xhtml") {
                                        pop();
                                    }
                                    foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
                                    continue starttagloop;
                                }
                                // else fall thru
                            default:
                                if ("http://www.w3.org/2000/svg" == currNs) {
                                    attributes.adjustForSvg();
                                    if (selfClosing) {
                                        appendVoidElementToCurrentMayFosterCamelCase(
                                                currNs, elementName, attributes);
                                        selfClosing = false;
                                    } else {
                                        appendToCurrentNodeAndPushElementMayFosterCamelCase(
                                                currNs, elementName, attributes);
                                    }
                                    attributes = null; // CPP
                                    break starttagloop;
                                } else {
                                    attributes.adjustForMath();
                                    if (selfClosing) {
                                        appendVoidElementToCurrentMayFoster(
                                                currNs, elementName, attributes);
                                        selfClosing = false;
                                    } else {
                                        appendToCurrentNodeAndPushElementMayFosterNoScoping(
                                                currNs, elementName, attributes);
                                    }
                                    attributes = null; // CPP
                                    break starttagloop;
                                }
                        }
                    }
                default:
                    switch (mode) {
                        case IN_TABLE_BODY:
                            switch (group) {
                                case TR:
                                    clearStackBackTo(findLastInTableScopeOrRootTbodyTheadTfoot());
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    mode = IN_ROW;
                                    attributes = null; // CPP
                                    break starttagloop;
                                case TD_OR_TH:
                                    err("\u201C" + name
                                            + "\u201D start tag in table body.");
                                    clearStackBackTo(findLastInTableScopeOrRootTbodyTheadTfoot());
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            ElementName.TR,
                                            HtmlAttributes.EMPTY_ATTRIBUTES);
                                    mode = IN_ROW;
                                    continue;
                                case CAPTION:
                                case COL:
                                case COLGROUP:
                                case TBODY_OR_THEAD_OR_TFOOT:
                                    eltPos = findLastInTableScopeOrRootTbodyTheadTfoot();
                                    if (eltPos == 0) {
                                        err("Stray \u201C" + name
                                                + "\u201D start tag.");
                                        break starttagloop;
                                    } else {
                                        clearStackBackTo(eltPos);
                                        pop();
                                        mode = IN_TABLE;
                                        continue;
                                    }
                                default:
                                    // fall through to IN_TABLE
                            }
                        case IN_ROW:
                            switch (group) {
                                case TD_OR_TH:
                                    clearStackBackTo(findLastOrRoot(TreeBuilder.TR));
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    mode = IN_CELL;
                                    insertMarker();
                                    attributes = null; // CPP
                                    break starttagloop;
                                case CAPTION:
                                case COL:
                                case COLGROUP:
                                case TBODY_OR_THEAD_OR_TFOOT:
                                case TR:
                                    eltPos = findLastOrRoot(TreeBuilder.TR);
                                    if (eltPos == 0) {
                                        assert fragment;
                                        err("No table row to close.");
                                        break starttagloop;
                                    }
                                    clearStackBackTo(eltPos);
                                    pop();
                                    mode = IN_TABLE_BODY;
                                    continue;
                                default:
                                    // fall through to IN_TABLE
                            }
                        case IN_TABLE:
                            intableloop: for (;;) {
                                switch (group) {
                                    case CAPTION:
                                        clearStackBackTo(findLastOrRoot(TreeBuilder.TABLE));
                                        insertMarker();
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        mode = IN_CAPTION;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case COLGROUP:
                                        clearStackBackTo(findLastOrRoot(TreeBuilder.TABLE));
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        mode = IN_COLUMN_GROUP;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case COL:
                                        clearStackBackTo(findLastOrRoot(TreeBuilder.TABLE));
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                ElementName.COLGROUP,
                                                HtmlAttributes.EMPTY_ATTRIBUTES);
                                        mode = IN_COLUMN_GROUP;
                                        continue starttagloop;
                                    case TBODY_OR_THEAD_OR_TFOOT:
                                        clearStackBackTo(findLastOrRoot(TreeBuilder.TABLE));
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        mode = IN_TABLE_BODY;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case TR:
                                    case TD_OR_TH:
                                        clearStackBackTo(findLastOrRoot(TreeBuilder.TABLE));
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                ElementName.TBODY,
                                                HtmlAttributes.EMPTY_ATTRIBUTES);
                                        mode = IN_TABLE_BODY;
                                        continue starttagloop;
                                    case TABLE:
                                        err("Start tag for \u201Ctable\u201D seen but the previous \u201Ctable\u201D is still open.");
                                        eltPos = findLastInTableScope(name);
                                        if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                            assert fragment;
                                            break starttagloop;
                                        }
                                        generateImpliedEndTags();
                                        // XXX is the next if dead code?
                                        if (!isCurrent("table")) {
                                            err("Unclosed elements on stack.");
                                        }
                                        while (currentPtr >= eltPos) {
                                            pop();
                                        }
                                        resetTheInsertionMode();
                                        continue starttagloop;
                                    case SCRIPT:
                                        // XXX need to manage much more stuff
                                        // here if
                                        // supporting
                                        // document.write()
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.SCRIPT_DATA, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case STYLE:
                                        appendToCurrentNodeAndPushElement(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.RAWTEXT, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case INPUT:
                                        if (!Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                                                "hidden",
                                                attributes.getValue(AttributeName.TYPE))) {
                                            break intableloop;
                                        }
                                        appendVoidElementToCurrent(
                                                "http://www.w3.org/1999/xhtml",
                                                name, attributes, formPointer);
                                        selfClosing = false;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case FORM:
                                        if (formPointer != null) {
                                            err("Saw a \u201Cform\u201D start tag, but there was already an active \u201Cform\u201D element. Nested forms are not allowed. Ignoring the tag.");
                                            break starttagloop;
                                        } else {
                                            err("Start tag \u201Cform\u201D seen in \u201Ctable\u201D.");
                                            appendVoidFormToCurrent(attributes);
                                            attributes = null; // CPP
                                            break starttagloop;
                                        }
                                    default:
                                        err("Start tag \u201C"
                                                + name
                                                + "\u201D seen in \u201Ctable\u201D.");
                                        // fall through to IN_BODY
                                        break intableloop;
                                }
                            }
                        case IN_CAPTION:
                            switch (group) {
                                case CAPTION:
                                case COL:
                                case COLGROUP:
                                case TBODY_OR_THEAD_OR_TFOOT:
                                case TR:
                                case TD_OR_TH:
                                    err("Stray \u201C"
                                            + name
                                            + "\u201D start tag in \u201Ccaption\u201D.");
                                    eltPos = findLastInTableScope("caption");
                                    if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                        break starttagloop;
                                    }
                                    generateImpliedEndTags();
                                    if (currentPtr != eltPos) {
                                        err("Unclosed elements on stack.");
                                    }
                                    while (currentPtr >= eltPos) {
                                        pop();
                                    }
                                    clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                                    mode = IN_TABLE;
                                    continue;
                                default:
                                    // fall through to IN_BODY
                            }
                        case IN_CELL:
                            switch (group) {
                                case CAPTION:
                                case COL:
                                case COLGROUP:
                                case TBODY_OR_THEAD_OR_TFOOT:
                                case TR:
                                case TD_OR_TH:
                                    eltPos = findLastInTableScopeTdTh();
                                    if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                        err("No cell to close.");
                                        break starttagloop;
                                    } else {
                                        closeTheCell(eltPos);
                                        continue;
                                    }
                                default:
                                    // fall through to IN_BODY
                            }
                        case FRAMESET_OK:
                            switch (group) {
                                case FRAMESET:
                                    if (mode == FRAMESET_OK) {
                                        if (currentPtr == 0
                                                || stack[1].group != BODY) {
                                            assert fragment;
                                            err("Stray \u201Cframeset\u201D start tag.");
                                            break starttagloop;
                                        } else {
                                            err("\u201Cframeset\u201D start tag seen.");
                                            detachFromParent(stack[1].node);
                                            while (currentPtr > 0) {
                                                pop();
                                            }
                                            appendToCurrentNodeAndPushElement(
                                                    "http://www.w3.org/1999/xhtml",
                                                    elementName, attributes);
                                            mode = IN_FRAMESET;
                                            attributes = null; // CPP
                                            break starttagloop;
                                        }
                                    } else {
                                        err("Stray \u201Cframeset\u201D start tag.");
                                        break starttagloop;
                                    }
                                    // NOT falling through!
                                case PRE_OR_LISTING:
                                case LI:
                                case DD_OR_DT:
                                case BUTTON:
                                case MARQUEE_OR_APPLET:
                                case OBJECT:
                                case TABLE:
                                case AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR:
                                case BR:
                                case EMBED_OR_IMG:
                                case INPUT:
                                case KEYGEN:
                                case HR:
                                case TEXTAREA:
                                case XMP:
                                case IFRAME:
                                case SELECT:
                                    if (mode == FRAMESET_OK) {
                                        framesetOk = false;
                                        mode = IN_BODY;
                                    }
                                    // fall through to IN_BODY
                                default:
                                    // fall through to IN_BODY
                            }
                        case IN_BODY:
                            inbodyloop: for (;;) {
                                switch (group) {
                                    case HTML:
                                        err("Stray \u201Chtml\u201D start tag.");
                                        addAttributesToHtml(attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case BASE:
                                    case LINK:
                                    case META:
                                    case STYLE:
                                    case SCRIPT:
                                    case TITLE:
                                    case COMMAND:
                                        // Fall through to IN_HEAD
                                        break inbodyloop;
                                    case BODY:
                                        err("\u201Cbody\u201D start tag found but the \u201Cbody\u201D element is already open.");
                                        addAttributesToBody(attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case P:
                                    case DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
                                    case UL_OR_OL_OR_DL:
                                    case ADDRESS_OR_DIR_OR_ARTICLE_OR_ASIDE_OR_DATAGRID_OR_DETAILS_OR_HGROUP_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_NAV_OR_SECTION:
                                        implicitlyCloseP();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
                                        implicitlyCloseP();
                                        if (stack[currentPtr].group == H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6) {
                                            err("Heading cannot be a child of another heading.");
                                            pop();
                                        }
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case FIELDSET:
                                        implicitlyCloseP();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes,
                                                formPointer);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case PRE_OR_LISTING:
                                        implicitlyCloseP();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        needToDropLF = true;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case FORM:
                                        if (formPointer != null) {
                                            err("Saw a \u201Cform\u201D start tag, but there was already an active \u201Cform\u201D element. Nested forms are not allowed. Ignoring the tag.");
                                            break starttagloop;
                                        } else {
                                            implicitlyCloseP();
                                            appendToCurrentNodeAndPushFormElementMayFoster(attributes);
                                            attributes = null; // CPP
                                            break starttagloop;
                                        }
                                    case LI:
                                    case DD_OR_DT:
                                        eltPos = currentPtr;
                                        for (;;) {
                                            StackNode<T> node = stack[eltPos]; // weak
                                                                               // ref
                                            if (node.group == group) { // LI or
                                                // DD_OR_DT
                                                generateImpliedEndTagsExceptFor(node.name);
                                                if (eltPos != currentPtr) {
                                                    err("Unclosed elements inside a list.");
                                                }
                                                while (currentPtr >= eltPos) {
                                                    pop();
                                                }
                                                break;
                                            } else if (node.scoping
                                                    || (node.special
                                                            && node.name != "p"
                                                            && node.name != "address" && node.name != "div")) {
                                                break;
                                            }
                                            eltPos--;
                                        }
                                        implicitlyCloseP();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case PLAINTEXT:
                                        implicitlyCloseP();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.PLAINTEXT,
                                                elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case A:
                                        int activeAPos = findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker("a");
                                        if (activeAPos != -1) {
                                            err("An \u201Ca\u201D start tag seen with already an active \u201Ca\u201D element.");
                                            StackNode<T> activeA = listOfActiveFormattingElements[activeAPos];
                                            activeA.retain();
                                            adoptionAgencyEndTag("a");
                                            removeFromStack(activeA);
                                            activeAPos = findInListOfActiveFormattingElements(activeA);
                                            if (activeAPos != -1) {
                                                removeFromListOfActiveFormattingElements(activeAPos);
                                            }
                                            activeA.release();
                                        }
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushFormattingElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
                                    case FONT:
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushFormattingElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case NOBR:
                                        reconstructTheActiveFormattingElements();
                                        if (TreeBuilder.NOT_FOUND_ON_STACK != findLastInScope("nobr")) {
                                            err("\u201Cnobr\u201D start tag seen when there was an open \u201Cnobr\u201D element in scope.");
                                            adoptionAgencyEndTag("nobr");
                                        }
                                        appendToCurrentNodeAndPushFormattingElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case BUTTON:
                                        eltPos = findLastInScope(name);
                                        if (eltPos != TreeBuilder.NOT_FOUND_ON_STACK) {
                                            err("\u201Cbutton\u201D start tag seen when there was an open \u201Cbutton\u201D element in scope.");
                                            generateImpliedEndTags();
                                            if (!isCurrent("button")) {
                                                err("There was an open \u201Cbutton\u201D element in scope with unclosed children.");
                                            }
                                            while (currentPtr >= eltPos) {
                                                pop();
                                            }
                                            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                                            continue starttagloop;
                                        } else {
                                            reconstructTheActiveFormattingElements();
                                            appendToCurrentNodeAndPushElementMayFoster(
                                                    "http://www.w3.org/1999/xhtml",
                                                    elementName, attributes,
                                                    formPointer);
                                            insertMarker();
                                            attributes = null; // CPP
                                            break starttagloop;
                                        }
                                    case OBJECT:
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes,
                                                formPointer);
                                        insertMarker();
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case MARQUEE_OR_APPLET:
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        insertMarker();
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case TABLE:
                                        // The only quirk. Blame Hixie and
                                        // Acid2.
                                        if (!quirks) {
                                            implicitlyCloseP();
                                        }
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        mode = IN_TABLE;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case BR:
                                    case EMBED_OR_IMG:
                                    case AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR:
                                        reconstructTheActiveFormattingElements();
                                        // FALL THROUGH to PARAM_OR_SOURCE
                                    case PARAM_OR_SOURCE:
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        selfClosing = false;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case HR:
                                        implicitlyCloseP();
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        selfClosing = false;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case IMAGE:
                                        err("Saw a start tag \u201Cimage\u201D.");
                                        elementName = ElementName.IMG;
                                        continue starttagloop;
                                    case KEYGEN:
                                    case INPUT:
                                        reconstructTheActiveFormattingElements();
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                name, attributes, formPointer);
                                        selfClosing = false;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case ISINDEX:
                                        err("\u201Cisindex\u201D seen.");
                                        if (formPointer != null) {
                                            break starttagloop;
                                        }
                                        implicitlyCloseP();
                                        HtmlAttributes formAttrs = new HtmlAttributes(
                                                0);
                                        int actionIndex = attributes.getIndex(AttributeName.ACTION);
                                        if (actionIndex > -1) {
                                            formAttrs.addAttribute(
                                                    AttributeName.ACTION,
                                                    attributes.getValue(actionIndex)
                                                    // [NOCPP[
                                                    , XmlViolationPolicy.ALLOW
                                            // ]NOCPP]
                                            );
                                        }
                                        appendToCurrentNodeAndPushFormElementMayFoster(formAttrs);
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                ElementName.HR,
                                                HtmlAttributes.EMPTY_ATTRIBUTES);
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                ElementName.LABEL,
                                                HtmlAttributes.EMPTY_ATTRIBUTES);
                                        int promptIndex = attributes.getIndex(AttributeName.PROMPT);
                                        if (promptIndex > -1) {
                                            char[] prompt = Portability.newCharArrayFromString(attributes.getValue(promptIndex));
                                            appendCharacters(
                                                    stack[currentPtr].node,
                                                    prompt, 0, prompt.length);
                                            Portability.releaseArray(prompt);
                                        } else {
                                            // XXX localization
                                            appendCharacters(
                                                    stack[currentPtr].node,
                                                    TreeBuilder.ISINDEX_PROMPT,
                                                    0,
                                                    TreeBuilder.ISINDEX_PROMPT.length);
                                        }
                                        HtmlAttributes inputAttributes = new HtmlAttributes(
                                                0);
                                        inputAttributes.addAttribute(
                                                AttributeName.NAME,
                                                Portability.newStringFromLiteral("isindex")
                                                // [NOCPP[
                                                , XmlViolationPolicy.ALLOW
                                        // ]NOCPP]
                                        );
                                        for (int i = 0; i < attributes.getLength(); i++) {
                                            AttributeName attributeQName = attributes.getAttributeName(i);
                                            if (AttributeName.NAME == attributeQName
                                                    || AttributeName.PROMPT == attributeQName) {
                                                attributes.releaseValue(i);
                                            } else if (AttributeName.ACTION != attributeQName) {
                                                inputAttributes.addAttribute(
                                                        attributeQName,
                                                        attributes.getValue(i)
                                                        // [NOCPP[
                                                        ,
                                                        XmlViolationPolicy.ALLOW
                                                // ]NOCPP]

                                                );
                                            }
                                        }
                                        attributes.clearWithoutReleasingContents();
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                "input", inputAttributes,
                                                formPointer);
                                        // XXX localization
                                        pop(); // label
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                ElementName.HR,
                                                HtmlAttributes.EMPTY_ATTRIBUTES);
                                        pop(); // form
                                        selfClosing = false;
                                        // Portability.delete(formAttrs);
                                        // Portability.delete(inputAttributes);
                                        // Don't delete attributes, they are deleted later
                                        break starttagloop;
                                    case TEXTAREA:
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes,
                                                formPointer);
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.RCDATA, elementName);
                                        originalMode = mode;
                                        mode = TEXT;
                                        needToDropLF = true;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case XMP:
                                        implicitlyCloseP();
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.RAWTEXT, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case NOSCRIPT:
                                        if (!scriptingEnabled) {
                                            reconstructTheActiveFormattingElements();
                                            appendToCurrentNodeAndPushElementMayFoster(
                                                    "http://www.w3.org/1999/xhtml",
                                                    elementName, attributes);
                                            attributes = null; // CPP
                                            break starttagloop;
                                        } else {
                                            // fall through
                                        }
                                    case NOFRAMES:
                                    case IFRAME:
                                    case NOEMBED:
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.RAWTEXT, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case SELECT:
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes,
                                                formPointer);
                                        switch (mode) {
                                            case IN_TABLE:
                                            case IN_CAPTION:
                                            case IN_COLUMN_GROUP:
                                            case IN_TABLE_BODY:
                                            case IN_ROW:
                                            case IN_CELL:
                                                mode = IN_SELECT_IN_TABLE;
                                                break;
                                            default:
                                                mode = IN_SELECT;
                                                break;
                                        }
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case OPTGROUP:
                                    case OPTION:
                                        /*
                                         * If the stack of open elements has an
                                         * option element in scope, then act as
                                         * if an end tag with the tag name
                                         * "option" had been seen.
                                         */
                                        if (findLastInScope("option") != TreeBuilder.NOT_FOUND_ON_STACK) {
                                            optionendtagloop: for (;;) {
                                                if (isCurrent("option")) {
                                                    pop();
                                                    break optionendtagloop;
                                                }

                                                eltPos = currentPtr;
                                                for (;;) {
                                                    if (stack[eltPos].name == "option") {
                                                        generateImpliedEndTags();
                                                        if (!isCurrent("option")) {
                                                            err("End tag \u201C"
                                                                    + name
                                                                    + "\u201D seen but there were unclosed elements.");
                                                        }
                                                        while (currentPtr >= eltPos) {
                                                            pop();
                                                        }
                                                        break optionendtagloop;
                                                    }
                                                    eltPos--;
                                                }
                                            }
                                        }
                                        /*
                                         * Reconstruct the active formatting
                                         * elements, if any.
                                         */
                                        reconstructTheActiveFormattingElements();
                                        /*
                                         * Insert an HTML element for the token.
                                         */
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case RT_OR_RP:
                                        /*
                                         * If the stack of open elements has a
                                         * ruby element in scope, then generate
                                         * implied end tags. If the current node
                                         * is not then a ruby element, this is a
                                         * parse error; pop all the nodes from
                                         * the current node up to the node
                                         * immediately before the bottommost
                                         * ruby element on the stack of open
                                         * elements.
                                         * 
                                         * Insert an HTML element for the token.
                                         */
                                        eltPos = findLastInScope("ruby");
                                        if (eltPos != NOT_FOUND_ON_STACK) {
                                            generateImpliedEndTags();
                                        }
                                        if (eltPos != currentPtr) {
                                            err("Unclosed children in \u201Cruby\u201D.");
                                            while (currentPtr > eltPos) {
                                                pop();
                                            }
                                        }
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case MATH:
                                        reconstructTheActiveFormattingElements();
                                        attributes.adjustForMath();
                                        if (selfClosing) {
                                            appendVoidElementToCurrentMayFoster(
                                                    "http://www.w3.org/1998/Math/MathML",
                                                    elementName, attributes);
                                            selfClosing = false;
                                        } else {
                                            appendToCurrentNodeAndPushElementMayFoster(
                                                    "http://www.w3.org/1998/Math/MathML",
                                                    elementName, attributes);
                                            foreignFlag = TreeBuilder.IN_FOREIGN;
                                        }
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case SVG:
                                        reconstructTheActiveFormattingElements();
                                        attributes.adjustForSvg();
                                        if (selfClosing) {
                                            appendVoidElementToCurrentMayFosterCamelCase(
                                                    "http://www.w3.org/2000/svg",
                                                    elementName, attributes);
                                            selfClosing = false;
                                        } else {
                                            appendToCurrentNodeAndPushElementMayFoster(
                                                    "http://www.w3.org/2000/svg",
                                                    elementName, attributes);
                                            foreignFlag = TreeBuilder.IN_FOREIGN;
                                        }
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case CAPTION:
                                    case COL:
                                    case COLGROUP:
                                    case TBODY_OR_THEAD_OR_TFOOT:
                                    case TR:
                                    case TD_OR_TH:
                                    case FRAME:
                                    case FRAMESET:
                                    case HEAD:
                                        err("Stray start tag \u201C" + name
                                                + "\u201D.");
                                        break starttagloop;
                                    case OUTPUT_OR_LABEL:
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes,
                                                formPointer);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    default:
                                        reconstructTheActiveFormattingElements();
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                }
                            }
                        case IN_HEAD:
                            inheadloop: for (;;) {
                                switch (group) {
                                    case HTML:
                                        err("Stray \u201Chtml\u201D start tag.");
                                        addAttributesToHtml(attributes);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case BASE:
                                    case COMMAND:
                                        appendVoidElementToCurrentMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        selfClosing = false;
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case META:
                                    case LINK:
                                        // Fall through to IN_HEAD_NOSCRIPT
                                        break inheadloop;
                                    case TITLE:
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.RCDATA, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case NOSCRIPT:
                                        if (scriptingEnabled) {
                                            appendToCurrentNodeAndPushElement(
                                                    "http://www.w3.org/1999/xhtml",
                                                    elementName, attributes);
                                            originalMode = mode;
                                            mode = TEXT;
                                            tokenizer.setContentModelFlag(
                                                    Tokenizer.RAWTEXT,
                                                    elementName);
                                        } else {
                                            appendToCurrentNodeAndPushElementMayFoster(
                                                    "http://www.w3.org/1999/xhtml",
                                                    elementName, attributes);
                                            mode = IN_HEAD_NOSCRIPT;
                                        }
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case SCRIPT:
                                        // XXX need to manage much more stuff
                                        // here if
                                        // supporting
                                        // document.write()
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.SCRIPT_DATA, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case STYLE:
                                    case NOFRAMES:
                                        appendToCurrentNodeAndPushElementMayFoster(
                                                "http://www.w3.org/1999/xhtml",
                                                elementName, attributes);
                                        originalMode = mode;
                                        mode = TEXT;
                                        tokenizer.setContentModelFlag(
                                                Tokenizer.RAWTEXT, elementName);
                                        attributes = null; // CPP
                                        break starttagloop;
                                    case HEAD:
                                        /* Parse error. */
                                        err("Start tag for \u201Chead\u201D seen when \u201Chead\u201D was already open.");
                                        /* Ignore the token. */
                                        break starttagloop;
                                    default:
                                        pop();
                                        mode = AFTER_HEAD;
                                        continue starttagloop;
                                }
                            }
                        case IN_HEAD_NOSCRIPT:
                            switch (group) {
                                case HTML:
                                    // XXX did Hixie really mean to omit "base"
                                    // here?
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case LINK:
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    attributes = null; // CPP
                                    break starttagloop;
                                case META:
                                    checkMetaCharset(attributes);
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    attributes = null; // CPP
                                    break starttagloop;
                                case STYLE:
                                case NOFRAMES:
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.RAWTEXT, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case HEAD:
                                    err("Start tag for \u201Chead\u201D seen when \u201Chead\u201D was already open.");
                                    break starttagloop;
                                case NOSCRIPT:
                                    err("Start tag for \u201Cnoscript\u201D seen when \u201Cnoscript\u201D was already open.");
                                    break starttagloop;
                                default:
                                    err("Bad start tag in \u201C" + name
                                            + "\u201D in \u201Chead\u201D.");
                                    pop();
                                    mode = IN_HEAD;
                                    continue;
                            }
                        case IN_COLUMN_GROUP:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case COL:
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    if (currentPtr == 0) {
                                        assert fragment;
                                        err("Garbage in \u201Ccolgroup\u201D fragment.");
                                        break starttagloop;
                                    }
                                    pop();
                                    mode = IN_TABLE;
                                    continue;
                            }
                        case IN_SELECT_IN_TABLE:
                            switch (group) {
                                case CAPTION:
                                case TBODY_OR_THEAD_OR_TFOOT:
                                case TR:
                                case TD_OR_TH:
                                case TABLE:
                                    err("\u201C"
                                            + name
                                            + "\u201D start tag with \u201Cselect\u201D open.");
                                    eltPos = findLastInTableScope("select");
                                    if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                        assert fragment;
                                        break starttagloop; // http://www.w3.org/Bugs/Public/show_bug.cgi?id=8375
                                    }
                                    while (currentPtr >= eltPos) {
                                        pop();
                                    }
                                    resetTheInsertionMode();
                                    continue;
                                default:
                                    // fall through to IN_SELECT
                            }
                        case IN_SELECT:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case OPTION:
                                    if (isCurrent("option")) {
                                        pop();
                                    }
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case OPTGROUP:
                                    if (isCurrent("option")) {
                                        pop();
                                    }
                                    if (isCurrent("optgroup")) {
                                        pop();
                                    }
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case SELECT:
                                    err("\u201Cselect\u201D start tag where end tag expected.");
                                    eltPos = findLastInTableScope(name);
                                    if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                        assert fragment;
                                        err("No \u201Cselect\u201D in table scope.");
                                        break starttagloop;
                                    } else {
                                        while (currentPtr >= eltPos) {
                                            pop();
                                        }
                                        resetTheInsertionMode();
                                        break starttagloop;
                                    }
                                case INPUT:
                                case TEXTAREA:
                                case KEYGEN:
                                    err("\u201C"
                                            + name
                                            + "\u201D start tag seen in \u201Cselect\2201D.");
                                    eltPos = findLastInTableScope("select");
                                    if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                        assert fragment;
                                        break starttagloop;
                                    }
                                    while (currentPtr >= eltPos) {
                                        pop();
                                    }
                                    resetTheInsertionMode();
                                    continue;
                                case SCRIPT:
                                    // XXX need to manage much more stuff
                                    // here if
                                    // supporting
                                    // document.write()
                                    appendToCurrentNodeAndPushElementMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.SCRIPT_DATA, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    err("Stray \u201C" + name
                                            + "\u201D start tag.");
                                    break starttagloop;
                            }
                        case AFTER_BODY:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    err("Stray \u201C" + name
                                            + "\u201D start tag.");
                                    mode = framesetOk ? FRAMESET_OK : IN_BODY;
                                    continue;
                            }
                        case IN_FRAMESET:
                            switch (group) {
                                case FRAMESET:
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case FRAME:
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    // fall through to AFTER_FRAMESET
                            }
                        case AFTER_FRAMESET:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case NOFRAMES:
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.RAWTEXT, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    err("Stray \u201C" + name
                                            + "\u201D start tag.");
                                    break starttagloop;
                            }
                        case INITIAL:
                            /*
                             * Parse error.
                             */
                            // [NOCPP[
                            switch (doctypeExpectation) {
                                case AUTO:
                                    err("Start tag seen without seeing a doctype first. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                                    break;
                                case HTML:
                                    err("Start tag seen without seeing a doctype first. Expected \u201C<!DOCTYPE html>\u201D.");
                                    break;
                                case HTML401_STRICT:
                                    err("Start tag seen without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                                    break;
                                case HTML401_TRANSITIONAL:
                                    err("Start tag seen without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                                    break;
                                case NO_DOCTYPE_ERRORS:
                            }
                            // ]NOCPP]
                            /*
                             * 
                             * Set the document to quirks mode.
                             */
                            documentModeInternal(DocumentMode.QUIRKS_MODE,
                                    null, null, false);
                            /*
                             * Then, switch to the root element mode of the tree
                             * construction stage
                             */
                            mode = BEFORE_HTML;
                            /*
                             * and reprocess the current token.
                             */
                            continue;
                        case BEFORE_HTML:
                            switch (group) {
                                case HTML:
                                    // optimize error check and streaming SAX by
                                    // hoisting
                                    // "html" handling here.
                                    if (attributes == HtmlAttributes.EMPTY_ATTRIBUTES) {
                                        // This has the right magic side effect
                                        // that
                                        // it
                                        // makes attributes in SAX Tree mutable.
                                        appendHtmlElementToDocumentAndPush();
                                    } else {
                                        appendHtmlElementToDocumentAndPush(attributes);
                                    }
                                    // XXX application cache should fire here
                                    mode = BEFORE_HEAD;
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    /*
                                     * Create an HTMLElement node with the tag
                                     * name html, in the HTML namespace. Append
                                     * it to the Document object.
                                     */
                                    appendHtmlElementToDocumentAndPush();
                                    /* Switch to the main mode */
                                    mode = BEFORE_HEAD;
                                    /*
                                     * reprocess the current token.
                                     */
                                    continue;
                            }
                        case BEFORE_HEAD:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case HEAD:
                                    /*
                                     * A start tag whose tag name is "head"
                                     * 
                                     * Create an element for the token.
                                     * 
                                     * Set the head element pointer to this new
                                     * element node.
                                     * 
                                     * Append the new element to the current
                                     * node and push it onto the stack of open
                                     * elements.
                                     */
                                    appendToCurrentNodeAndPushHeadElement(attributes);
                                    /*
                                     * 
                                     * Change the insertion mode to "in head".
                                     */
                                    mode = IN_HEAD;
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:

                                    /*
                                     * Any other start tag token
                                     */

                                    /*
                                     * Act as if a start tag token with the tag
                                     * name "head" and no attributes had been
                                     * seen,
                                     */
                                    appendToCurrentNodeAndPushHeadElement(HtmlAttributes.EMPTY_ATTRIBUTES);
                                    mode = IN_HEAD;
                                    /*
                                     * then reprocess the current token.
                                     * 
                                     * This will result in an empty head element
                                     * being generated, with the current token
                                     * being reprocessed in the "after head"
                                     * insertion mode.
                                     */
                                    continue;
                            }
                        case AFTER_HEAD:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case BODY:
                                    if (attributes.getLength() == 0) {
                                        // This has the right magic side effect
                                        // that
                                        // it
                                        // makes attributes in SAX Tree mutable.
                                        appendToCurrentNodeAndPushBodyElement();
                                    } else {
                                        appendToCurrentNodeAndPushBodyElement(attributes);
                                    }
                                    framesetOk = false;
                                    mode = IN_BODY;
                                    attributes = null; // CPP
                                    break starttagloop;
                                case FRAMESET:
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    mode = IN_FRAMESET;
                                    attributes = null; // CPP
                                    break starttagloop;
                                case BASE:
                                    err("\u201Cbase\u201D element outside \u201Chead\u201D.");
                                    pushHeadPointerOntoStack();
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    pop(); // head
                                    attributes = null; // CPP
                                    break starttagloop;
                                case LINK:
                                    err("\u201Clink\u201D element outside \u201Chead\u201D.");
                                    pushHeadPointerOntoStack();
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    pop(); // head
                                    attributes = null; // CPP
                                    break starttagloop;
                                case META:
                                    err("\u201Cmeta\u201D element outside \u201Chead\u201D.");
                                    checkMetaCharset(attributes);
                                    pushHeadPointerOntoStack();
                                    appendVoidElementToCurrentMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    selfClosing = false;
                                    pop(); // head
                                    attributes = null; // CPP
                                    break starttagloop;
                                case SCRIPT:
                                    err("\u201Cscript\u201D element between \u201Chead\u201D and \u201Cbody\u201D.");
                                    pushHeadPointerOntoStack();
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.SCRIPT_DATA, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case STYLE:
                                case NOFRAMES:
                                    err("\u201C"
                                            + name
                                            + "\u201D element between \u201Chead\u201D and \u201Cbody\u201D.");
                                    pushHeadPointerOntoStack();
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.RAWTEXT, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case TITLE:
                                    err("\u201Ctitle\u201D element outside \u201Chead\u201D.");
                                    pushHeadPointerOntoStack();
                                    appendToCurrentNodeAndPushElement(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.RCDATA, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                case HEAD:
                                    err("Stray start tag \u201Chead\u201D.");
                                    break starttagloop;
                                default:
                                    appendToCurrentNodeAndPushBodyElement();
                                    mode = FRAMESET_OK;
                                    continue;
                            }
                        case AFTER_AFTER_BODY:
                            switch (group) {
                                case HTML:
                                    err("Stray \u201Chtml\u201D start tag.");
                                    addAttributesToHtml(attributes);
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    err("Stray \u201C" + name
                                            + "\u201D start tag.");
                                    fatal();
                                    mode = framesetOk ? FRAMESET_OK : IN_BODY;
                                    continue;
                            }
                        case AFTER_AFTER_FRAMESET:
                            switch (group) {
                                case NOFRAMES:
                                    appendToCurrentNodeAndPushElementMayFoster(
                                            "http://www.w3.org/1999/xhtml",
                                            elementName, attributes);
                                    originalMode = mode;
                                    mode = TEXT;
                                    tokenizer.setContentModelFlag(
                                            Tokenizer.SCRIPT_DATA, elementName);
                                    attributes = null; // CPP
                                    break starttagloop;
                                default:
                                    err("Stray \u201C" + name
                                            + "\u201D start tag.");
                                    break starttagloop;
                            }
                        case TEXT:
                            assert false;
                            break starttagloop; // Avoid infinite loop if the assertion fails
                    }
            }
        }
        if (needsPostProcessing && foreignFlag == TreeBuilder.IN_FOREIGN
                && !hasForeignInScope()) {
            /*
             * If, after doing so, the insertion mode is still "in foreign
             * content", but there is no element in scope that has a namespace
             * other than the HTML namespace, switch the insertion mode to the
             * secondary insertion mode.
             */
            foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
        }
        if (selfClosing) {
            err("Self-closing syntax (\u201C/>\u201D) used on a non-void HTML element. Ignoring the slash and treating as a start tag.");
        }
        if (attributes != HtmlAttributes.EMPTY_ATTRIBUTES) {
            Portability.delete(attributes);
        }
    }

    /**
     * 
     * <p>
     * C++ memory note: The return value must be released.
     * 
     * @return
     * @throws SAXException
     * @throws StopSniffingException
     */
    public static String extractCharsetFromContent(String attributeValue) {
        // This is a bit ugly. Converting the string to char array in order to
        // make the portability layer smaller.
        int charsetState = CHARSET_INITIAL;
        int start = -1;
        int end = -1;
        char[] buffer = Portability.newCharArrayFromString(attributeValue);

        charsetloop: for (int i = 0; i < buffer.length; i++) {
            char c = buffer[i];
            switch (charsetState) {
                case CHARSET_INITIAL:
                    switch (c) {
                        case 'c':
                        case 'C':
                            charsetState = CHARSET_C;
                            continue;
                        default:
                            continue;
                    }
                case CHARSET_C:
                    switch (c) {
                        case 'h':
                        case 'H':
                            charsetState = CHARSET_H;
                            continue;
                        default:
                            charsetState = CHARSET_INITIAL;
                            continue;
                    }
                case CHARSET_H:
                    switch (c) {
                        case 'a':
                        case 'A':
                            charsetState = CHARSET_A;
                            continue;
                        default:
                            charsetState = CHARSET_INITIAL;
                            continue;
                    }
                case CHARSET_A:
                    switch (c) {
                        case 'r':
                        case 'R':
                            charsetState = CHARSET_R;
                            continue;
                        default:
                            charsetState = CHARSET_INITIAL;
                            continue;
                    }
                case CHARSET_R:
                    switch (c) {
                        case 's':
                        case 'S':
                            charsetState = CHARSET_S;
                            continue;
                        default:
                            charsetState = CHARSET_INITIAL;
                            continue;
                    }
                case CHARSET_S:
                    switch (c) {
                        case 'e':
                        case 'E':
                            charsetState = CHARSET_E;
                            continue;
                        default:
                            charsetState = CHARSET_INITIAL;
                            continue;
                    }
                case CHARSET_E:
                    switch (c) {
                        case 't':
                        case 'T':
                            charsetState = CHARSET_T;
                            continue;
                        default:
                            charsetState = CHARSET_INITIAL;
                            continue;
                    }
                case CHARSET_T:
                    switch (c) {
                        case '\t':
                        case '\n':
                        case '\u000C':
                        case '\r':
                        case ' ':
                            continue;
                        case '=':
                            charsetState = CHARSET_EQUALS;
                            continue;
                        default:
                            return null;
                    }
                case CHARSET_EQUALS:
                    switch (c) {
                        case '\t':
                        case '\n':
                        case '\u000C':
                        case '\r':
                        case ' ':
                            continue;
                        case '\'':
                            start = i + 1;
                            charsetState = CHARSET_SINGLE_QUOTED;
                            continue;
                        case '\"':
                            start = i + 1;
                            charsetState = CHARSET_DOUBLE_QUOTED;
                            continue;
                        default:
                            start = i;
                            charsetState = CHARSET_UNQUOTED;
                            continue;
                    }
                case CHARSET_SINGLE_QUOTED:
                    switch (c) {
                        case '\'':
                            end = i;
                            break charsetloop;
                        default:
                            continue;
                    }
                case CHARSET_DOUBLE_QUOTED:
                    switch (c) {
                        case '\"':
                            end = i;
                            break charsetloop;
                        default:
                            continue;
                    }
                case CHARSET_UNQUOTED:
                    switch (c) {
                        case '\t':
                        case '\n':
                        case '\u000C':
                        case '\r':
                        case ' ':
                        case ';':
                            end = i;
                            break charsetloop;
                        default:
                            continue;
                    }
            }
        }
        String charset = null;
        if (start != -1) {
            if (end == -1) {
                end = buffer.length;
            }
            charset = Portability.newStringFromBuffer(buffer, start, end
                    - start);
        }
        Portability.releaseArray(buffer);
        return charset;
    }

    private void checkMetaCharset(HtmlAttributes attributes)
            throws SAXException {
        String content = attributes.getValue(AttributeName.CONTENT);
        String internalCharsetLegacy = null;
        if (content != null) {
            internalCharsetLegacy = TreeBuilder.extractCharsetFromContent(content);
            // [NOCPP[
            if (errorHandler != null
                    && internalCharsetLegacy != null
                    && !Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                            "content-type",
                            attributes.getValue(AttributeName.HTTP_EQUIV))) {
                warn("Attribute \u201Ccontent\u201D would be sniffed as an internal character encoding declaration but there was no matching \u201Chttp-equiv='Content-Type'\u201D attribute.");
            }
            // ]NOCPP]
        }
        if (internalCharsetLegacy == null) {
            String internalCharsetHtml5 = attributes.getValue(AttributeName.CHARSET);
            if (internalCharsetHtml5 != null) {
                tokenizer.internalEncodingDeclaration(internalCharsetHtml5);
                requestSuspension();
            }
        } else {
            tokenizer.internalEncodingDeclaration(internalCharsetLegacy);
            Portability.releaseString(internalCharsetLegacy);
            requestSuspension();
        }
    }

    public final void endTag(ElementName elementName) throws SAXException {
        needToDropLF = false;
        int eltPos;
        endtagloop: for (;;) {
            int group = elementName.group;
            @Local String name = elementName.name;
            switch (mode) {
                case IN_ROW:
                    switch (group) {
                        case TR:
                            eltPos = findLastOrRoot(TreeBuilder.TR);
                            if (eltPos == 0) {
                                assert fragment;
                                err("No table row to close.");
                                break endtagloop;
                            }
                            clearStackBackTo(eltPos);
                            pop();
                            mode = IN_TABLE_BODY;
                            break endtagloop;
                        case TABLE:
                            eltPos = findLastOrRoot(TreeBuilder.TR);
                            if (eltPos == 0) {
                                assert fragment;
                                err("No table row to close.");
                                break endtagloop;
                            }
                            clearStackBackTo(eltPos);
                            pop();
                            mode = IN_TABLE_BODY;
                            continue;
                        case TBODY_OR_THEAD_OR_TFOOT:
                            if (findLastInTableScope(name) == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                                break endtagloop;
                            }
                            eltPos = findLastOrRoot(TreeBuilder.TR);
                            if (eltPos == 0) {
                                assert fragment;
                                err("No table row to close.");
                                break endtagloop;
                            }
                            clearStackBackTo(eltPos);
                            pop();
                            mode = IN_TABLE_BODY;
                            continue;
                        case BODY:
                        case CAPTION:
                        case COL:
                        case COLGROUP:
                        case HTML:
                        case TD_OR_TH:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                        default:
                            // fall through to IN_TABLE
                    }
                case IN_TABLE_BODY:
                    switch (group) {
                        case TBODY_OR_THEAD_OR_TFOOT:
                            eltPos = findLastOrRoot(name);
                            if (eltPos == 0) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                                break endtagloop;
                            }
                            clearStackBackTo(eltPos);
                            pop();
                            mode = IN_TABLE;
                            break endtagloop;
                        case TABLE:
                            eltPos = findLastInTableScopeOrRootTbodyTheadTfoot();
                            if (eltPos == 0) {
                                assert fragment;
                                err("Stray end tag \u201Ctable\u201D.");
                                break endtagloop;
                            }
                            clearStackBackTo(eltPos);
                            pop();
                            mode = IN_TABLE;
                            continue;
                        case BODY:
                        case CAPTION:
                        case COL:
                        case COLGROUP:
                        case HTML:
                        case TD_OR_TH:
                        case TR:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                        default:
                            // fall through to IN_TABLE
                    }
                case IN_TABLE:
                    switch (group) {
                        case TABLE:
                            eltPos = findLast("table");
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                assert fragment;
                                err("Stray end tag \u201Ctable\u201D.");
                                break endtagloop;
                            }
                            while (currentPtr >= eltPos) {
                                pop();
                            }
                            resetTheInsertionMode();
                            break endtagloop;
                        case BODY:
                        case CAPTION:
                        case COL:
                        case COLGROUP:
                        case HTML:
                        case TBODY_OR_THEAD_OR_TFOOT:
                        case TD_OR_TH:
                        case TR:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            // fall through to IN_BODY
                    }
                case IN_CAPTION:
                    switch (group) {
                        case CAPTION:
                            eltPos = findLastInTableScope("caption");
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                break endtagloop;
                            }
                            generateImpliedEndTags();
                            if (currentPtr != eltPos) {
                                err("Unclosed elements on stack.");
                            }
                            while (currentPtr >= eltPos) {
                                pop();
                            }
                            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                            mode = IN_TABLE;
                            break endtagloop;
                        case TABLE:
                            err("\u201Ctable\u201D closed but \u201Ccaption\u201D was still open.");
                            eltPos = findLastInTableScope("caption");
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                break endtagloop;
                            }
                            generateImpliedEndTags();
                            if (currentPtr != eltPos) {
                                err("Unclosed elements on stack.");
                            }
                            while (currentPtr >= eltPos) {
                                pop();
                            }
                            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                            mode = IN_TABLE;
                            continue;
                        case BODY:
                        case COL:
                        case COLGROUP:
                        case HTML:
                        case TBODY_OR_THEAD_OR_TFOOT:
                        case TD_OR_TH:
                        case TR:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                        default:
                            // fall through to IN_BODY
                    }
                case IN_CELL:
                    switch (group) {
                        case TD_OR_TH:
                            eltPos = findLastInTableScope(name);
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                                break endtagloop;
                            }
                            generateImpliedEndTags();
                            if (!isCurrent(name)) {
                                err("Unclosed elements.");
                            }
                            while (currentPtr >= eltPos) {
                                pop();
                            }
                            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                            mode = IN_ROW;
                            break endtagloop;
                        case TABLE:
                        case TBODY_OR_THEAD_OR_TFOOT:
                        case TR:
                            if (findLastInTableScope(name) == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                                break endtagloop;
                            }
                            closeTheCell(findLastInTableScopeTdTh());
                            continue;
                        case BODY:
                        case CAPTION:
                        case COL:
                        case COLGROUP:
                        case HTML:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                        default:
                            // fall through to IN_BODY
                    }
                case FRAMESET_OK:
                case IN_BODY:
                    switch (group) {
                        case BODY:
                            if (!isSecondOnStackBody()) {
                                assert fragment;
                                err("Stray end tag \u201Cbody\u201D.");
                                break endtagloop;
                            }
                            assert currentPtr >= 1;
                            if (errorHandler != null) {
                                uncloseloop1: for (int i = 2; i <= currentPtr; i++) {
                                    switch (stack[i].group) {
                                        case DD_OR_DT:
                                        case LI:
                                        case OPTGROUP:
                                        case OPTION: // is this possible?
                                        case P:
                                        case RT_OR_RP:
                                        case TD_OR_TH:
                                        case TBODY_OR_THEAD_OR_TFOOT:
                                            break;
                                        default:
                                            err("End tag for \u201Cbody\u201D seen but there were unclosed elements.");
                                            break uncloseloop1;
                                    }
                                }
                            }
                            mode = AFTER_BODY;
                            break endtagloop;
                        case HTML:
                            if (!isSecondOnStackBody()) {
                                assert fragment;
                                err("Stray end tag \u201Chtml\u201D.");
                                break endtagloop;
                            }
                            if (errorHandler != null) {
                                uncloseloop2: for (int i = 0; i <= currentPtr; i++) {
                                    switch (stack[i].group) {
                                        case DD_OR_DT:
                                        case LI:
                                        case P:
                                        case TBODY_OR_THEAD_OR_TFOOT:
                                        case TD_OR_TH:
                                        case BODY:
                                        case HTML:
                                            break;
                                        default:
                                            err("End tag for \u201Chtml\u201D seen but there were unclosed elements.");
                                            break uncloseloop2;
                                    }
                                }
                            }
                            mode = AFTER_BODY;
                            continue;
                        case DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
                        case UL_OR_OL_OR_DL:
                        case PRE_OR_LISTING:
                        case FIELDSET:
                        case ADDRESS_OR_DIR_OR_ARTICLE_OR_ASIDE_OR_DATAGRID_OR_DETAILS_OR_HGROUP_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_NAV_OR_SECTION:
                            eltPos = findLastInScope(name);
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                            } else {
                                generateImpliedEndTags();
                                if (!isCurrent(name)) {
                                    err("End tag \u201C"
                                            + name
                                            + "\u201D seen but there were unclosed elements.");
                                }
                                while (currentPtr >= eltPos) {
                                    pop();
                                }
                            }
                            break endtagloop;
                        case FORM:
                            if (formPointer == null) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                                break endtagloop;
                            }
                            Portability.releaseElement(formPointer);
                            formPointer = null;
                            eltPos = findLastInScope(name);
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                                break endtagloop;
                            }
                            generateImpliedEndTags();
                            if (!isCurrent(name)) {
                                err("End tag \u201C"
                                        + name
                                        + "\u201D seen but there were unclosed elements.");
                            }
                            removeFromStack(eltPos);
                            break endtagloop;
                        case P:
                            eltPos = findLastInScope("p");
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("No \u201Cp\u201D element in scope but a \u201Cp\u201D end tag seen.");
                                // XXX inline this case
                                if (foreignFlag == TreeBuilder.IN_FOREIGN) {
                                    err("HTML start tag \u201C"
                                            + name
                                            + "\u201D in a foreign namespace context.");
                                    while (stack[currentPtr].ns != "http://www.w3.org/1999/xhtml") {
                                        pop();
                                    }
                                    foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
                                }
                                appendVoidElementToCurrentMayFoster(
                                        "http://www.w3.org/1999/xhtml",
                                        elementName,
                                        HtmlAttributes.EMPTY_ATTRIBUTES);
                                break endtagloop;
                            }
                            generateImpliedEndTagsExceptFor("p");
                            assert eltPos != TreeBuilder.NOT_FOUND_ON_STACK;
                            if (eltPos != currentPtr) {
                                err("End tag for \u201Cp\u201D seen, but there were unclosed elements.");
                            }
                            while (currentPtr >= eltPos) {
                                pop();
                            }
                            break endtagloop;
                        case LI:
                            eltPos = findLastInListScope(name);
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("No \u201Cli\u201D element in list scope but a \u201Cli\u201D end tag seen.");
                            } else {
                                generateImpliedEndTagsExceptFor(name);
                                if (eltPos != currentPtr) {
                                    err("End tag for \u201Cli\u201D seen, but there were unclosed elements.");
                                }
                                while (currentPtr >= eltPos) {
                                    pop();
                                }
                            }
                            break endtagloop;
                        case DD_OR_DT:
                            eltPos = findLastInScope(name);
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("No \u201C"
                                        + name
                                        + "\u201D element in scope but a \u201C"
                                        + name + "\u201D end tag seen.");
                            } else {
                                generateImpliedEndTagsExceptFor(name);
                                if (eltPos != currentPtr) {
                                    err("End tag for \u201C"
                                            + name
                                            + "\u201D seen, but there were unclosed elements.");
                                }
                                while (currentPtr >= eltPos) {
                                    pop();
                                }
                            }
                            break endtagloop;
                        case H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
                            eltPos = findLastInScopeHn();
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                            } else {
                                generateImpliedEndTags();
                                if (!isCurrent(name)) {
                                    err("End tag \u201C"
                                            + name
                                            + "\u201D seen but there were unclosed elements.");
                                }
                                while (currentPtr >= eltPos) {
                                    pop();
                                }
                            }
                            break endtagloop;
                        case A:
                        case B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
                        case FONT:
                        case NOBR:
                            adoptionAgencyEndTag(name);
                            break endtagloop;
                        case BUTTON:
                        case OBJECT:
                        case MARQUEE_OR_APPLET:
                            eltPos = findLastInScope(name);
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                err("Stray end tag \u201C" + name + "\u201D.");
                            } else {
                                generateImpliedEndTags();
                                if (!isCurrent(name)) {
                                    err("End tag \u201C"
                                            + name
                                            + "\u201D seen but there were unclosed elements.");
                                }
                                while (currentPtr >= eltPos) {
                                    pop();
                                }
                                clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                            }
                            break endtagloop;
                        case BR:
                            err("End tag \u201Cbr\u201D.");
                            if (foreignFlag == TreeBuilder.IN_FOREIGN) {
                                err("HTML start tag \u201C"
                                        + name
                                        + "\u201D in a foreign namespace context.");
                                while (stack[currentPtr].ns != "http://www.w3.org/1999/xhtml") {
                                    pop();
                                }
                                foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
                            }
                            reconstructTheActiveFormattingElements();
                            appendVoidElementToCurrentMayFoster(
                                    "http://www.w3.org/1999/xhtml",
                                    elementName,
                                    HtmlAttributes.EMPTY_ATTRIBUTES);
                            break endtagloop;
                        case AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR:
                        case PARAM_OR_SOURCE:
                        case EMBED_OR_IMG:
                        case IMAGE:
                        case INPUT:
                        case KEYGEN: // XXX??
                        case HR:
                        case ISINDEX:
                        case IFRAME:
                        case NOEMBED: // XXX???
                        case NOFRAMES: // XXX??
                        case SELECT:
                        case TABLE:
                        case TEXTAREA: // XXX??
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                        case NOSCRIPT:
                            if (scriptingEnabled) {
                                err("Stray end tag \u201Cnoscript\u201D.");
                                break endtagloop;
                            } else {
                                // fall through
                            }
                        default:
                            if (isCurrent(name)) {
                                pop();
                                break endtagloop;
                            }

                            eltPos = currentPtr;
                            for (;;) {
                                StackNode<T> node = stack[eltPos];
                                if (node.name == name) {
                                    generateImpliedEndTags();
                                    if (!isCurrent(name)) {
                                        err("End tag \u201C"
                                                + name
                                                + "\u201D seen but there were unclosed elements.");
                                    }
                                    while (currentPtr >= eltPos) {
                                        pop();
                                    }
                                    break endtagloop;
                                } else if (node.scoping || node.special) {
                                    err("Stray end tag \u201C" + name
                                            + "\u201D.");
                                    break endtagloop;
                                }
                                eltPos--;
                            }
                    }
                case IN_COLUMN_GROUP:
                    switch (group) {
                        case COLGROUP:
                            if (currentPtr == 0) {
                                assert fragment;
                                err("Garbage in \u201Ccolgroup\u201D fragment.");
                                break endtagloop;
                            }
                            pop();
                            mode = IN_TABLE;
                            break endtagloop;
                        case COL:
                            err("Stray end tag \u201Ccol\u201D.");
                            break endtagloop;
                        default:
                            if (currentPtr == 0) {
                                assert fragment;
                                err("Garbage in \u201Ccolgroup\u201D fragment.");
                                break endtagloop;
                            }
                            pop();
                            mode = IN_TABLE;
                            continue;
                    }
                case IN_SELECT_IN_TABLE:
                    switch (group) {
                        case CAPTION:
                        case TABLE:
                        case TBODY_OR_THEAD_OR_TFOOT:
                        case TR:
                        case TD_OR_TH:
                            err("\u201C"
                                    + name
                                    + "\u201D end tag with \u201Cselect\u201D open.");
                            if (findLastInTableScope(name) != TreeBuilder.NOT_FOUND_ON_STACK) {
                                eltPos = findLastInTableScope("select");
                                if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                    assert fragment;
                                    break endtagloop; // http://www.w3.org/Bugs/Public/show_bug.cgi?id=8375
                                }
                                while (currentPtr >= eltPos) {
                                    pop();
                                }
                                resetTheInsertionMode();
                                continue;
                            } else {
                                break endtagloop;
                            }
                        default:
                            // fall through to IN_SELECT
                    }
                case IN_SELECT:
                    switch (group) {
                        case OPTION:
                            if (isCurrent("option")) {
                                pop();
                                break endtagloop;
                            } else {
                                err("Stray end tag \u201Coption\u201D");
                                break endtagloop;
                            }
                        case OPTGROUP:
                            if (isCurrent("option")
                                    && "optgroup" == stack[currentPtr - 1].name) {
                                pop();
                            }
                            if (isCurrent("optgroup")) {
                                pop();
                            } else {
                                err("Stray end tag \u201Coptgroup\u201D");
                            }
                            break endtagloop;
                        case SELECT:
                            eltPos = findLastInTableScope("select");
                            if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
                                assert fragment;
                                err("Stray end tag \u201Cselect\u201D");
                                break endtagloop;
                            }
                            while (currentPtr >= eltPos) {
                                pop();
                            }
                            resetTheInsertionMode();
                            break endtagloop;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D");
                            break endtagloop;
                    }
                case AFTER_BODY:
                    switch (group) {
                        case HTML:
                            if (fragment) {
                                err("Stray end tag \u201Chtml\u201D");
                                break endtagloop;
                            } else {
                                mode = AFTER_AFTER_BODY;
                                break endtagloop;
                            }
                        default:
                            err("Saw an end tag after \u201Cbody\u201D had been closed.");
                            mode = framesetOk ? FRAMESET_OK : IN_BODY;
                            continue;
                    }
                case IN_FRAMESET:
                    switch (group) {
                        case FRAMESET:
                            if (currentPtr == 0) {
                                assert fragment;
                                err("Stray end tag \u201Cframeset\u201D");
                                break endtagloop;
                            }
                            pop();
                            if ((!fragment) && !isCurrent("frameset")) {
                                mode = AFTER_FRAMESET;
                            }
                            break endtagloop;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D");
                            break endtagloop;
                    }
                case AFTER_FRAMESET:
                    switch (group) {
                        case HTML:
                            mode = AFTER_AFTER_FRAMESET;
                            break endtagloop;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D");
                            break endtagloop;
                    }
                case INITIAL:
                    /*
                     * Parse error.
                     */
                    // [NOCPP[
                    switch (doctypeExpectation) {
                        case AUTO:
                            err("End tag seen without seeing a doctype first. Expected e.g. \u201C<!DOCTYPE html>\u201D.");
                            break;
                        case HTML:
                            err("End tag seen without seeing a doctype first. Expected \u201C<!DOCTYPE html>\u201D.");
                            break;
                        case HTML401_STRICT:
                            err("End tag seen without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\u201D.");
                            break;
                        case HTML401_TRANSITIONAL:
                            err("End tag seen without seeing a doctype first. Expected \u201C<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\u201D.");
                            break;
                        case NO_DOCTYPE_ERRORS:
                    }
                    // ]NOCPP]
                    /*
                     * 
                     * Set the document to quirks mode.
                     */
                    documentModeInternal(DocumentMode.QUIRKS_MODE, null, null,
                            false);
                    /*
                     * Then, switch to the root element mode of the tree
                     * construction stage
                     */
                    mode = BEFORE_HTML;
                    /*
                     * and reprocess the current token.
                     */
                    continue;
                case BEFORE_HTML:
                    switch (group) {
                        case HEAD:
                        case BR:
                        case HTML:
                        case BODY:
                            /*
                             * Create an HTMLElement node with the tag name html, in the
                             * HTML namespace. Append it to the Document object.
                             */
                            appendHtmlElementToDocumentAndPush();
                            /* Switch to the main mode */
                            mode = BEFORE_HEAD;
                            /*
                             * reprocess the current token.
                             */
                            continue;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                    }
                case BEFORE_HEAD:
                    switch (group) {
                        case HEAD:
                        case BR:
                        case HTML:
                        case BODY:
                            appendToCurrentNodeAndPushHeadElement(HtmlAttributes.EMPTY_ATTRIBUTES);
                            mode = IN_HEAD;
                            continue;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                    }
                case IN_HEAD:
                    switch (group) {
                        case HEAD:
                            pop();
                            mode = AFTER_HEAD;
                            break endtagloop;
                        case BR:
                        case HTML:
                        case BODY:
                            pop();
                            mode = AFTER_HEAD;
                            continue;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                    }
                case IN_HEAD_NOSCRIPT:
                    switch (group) {
                        case NOSCRIPT:
                            pop();
                            mode = IN_HEAD;
                            break endtagloop;
                        case BR:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            pop();
                            mode = IN_HEAD;
                            continue;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                    }
                case AFTER_HEAD:
                    switch (group) {
                        case HTML:
                        case BODY:
                        case BR:
                            appendToCurrentNodeAndPushBodyElement();
                            mode = FRAMESET_OK;
                            continue;
                        default:
                            err("Stray end tag \u201C" + name + "\u201D.");
                            break endtagloop;
                    }
                case AFTER_AFTER_BODY:
                    err("Stray \u201C" + name + "\u201D end tag.");
                    mode = framesetOk ? FRAMESET_OK : IN_BODY;
                    continue;
                case AFTER_AFTER_FRAMESET:
                    err("Stray \u201C" + name + "\u201D end tag.");
                    mode = IN_FRAMESET;
                    continue;
                case TEXT:
                    // XXX need to manage insertion point here
                    pop();
                    if (originalMode == AFTER_HEAD) {
                        silentPop();
                    }
                    mode = originalMode;
                    break endtagloop;
            }
        }
        if (foreignFlag == TreeBuilder.IN_FOREIGN && !hasForeignInScope()) {
            /*
             * If, after doing so, the insertion mode is still "in foreign
             * content", but there is no element in scope that has a namespace
             * other than the HTML namespace, switch the insertion mode to the
             * secondary insertion mode.
             */
            foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
        }
    }

    private int findLastInTableScopeOrRootTbodyTheadTfoot() {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].group == TreeBuilder.TBODY_OR_THEAD_OR_TFOOT) {
                return i;
            }
        }
        return 0;
    }

    private int findLast(@Local String name) {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].name == name) {
                return i;
            }
        }
        return TreeBuilder.NOT_FOUND_ON_STACK;
    }

    private int findLastInTableScope(@Local String name) {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].name == name) {
                return i;
            } else if (stack[i].name == "table") {
                return TreeBuilder.NOT_FOUND_ON_STACK;
            }
        }
        return TreeBuilder.NOT_FOUND_ON_STACK;
    }

    private int findLastInScope(@Local String name) {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].name == name) {
                return i;
            } else if (stack[i].scoping) {
                return TreeBuilder.NOT_FOUND_ON_STACK;
            }
        }
        return TreeBuilder.NOT_FOUND_ON_STACK;
    }

    private int findLastInListScope(@Local String name) {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].name == name) {
                return i;
            } else if (stack[i].scoping || stack[i].name == "ul" || stack[i].name == "ol") {
                return TreeBuilder.NOT_FOUND_ON_STACK;
            }
        }
        return TreeBuilder.NOT_FOUND_ON_STACK;
    }
    
    private int findLastInScopeHn() {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].group == TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6) {
                return i;
            } else if (stack[i].scoping) {
                return TreeBuilder.NOT_FOUND_ON_STACK;
            }
        }
        return TreeBuilder.NOT_FOUND_ON_STACK;
    }

    private boolean hasForeignInScope() {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].ns != "http://www.w3.org/1999/xhtml") {
                return true;
            } else if (stack[i].scoping) {
                return false;
            }
        }
        return false;
    }

    private void generateImpliedEndTagsExceptFor(@Local String name)
            throws SAXException {
        for (;;) {
            StackNode<T> node = stack[currentPtr];
            switch (node.group) {
                case P:
                case LI:
                case DD_OR_DT:
                case OPTION:
                case OPTGROUP:
                case RT_OR_RP:
                    if (node.name == name) {
                        return;
                    }
                    pop();
                    continue;
                default:
                    return;
            }
        }
    }

    private void generateImpliedEndTags() throws SAXException {
        for (;;) {
            switch (stack[currentPtr].group) {
                case P:
                case LI:
                case DD_OR_DT:
                case OPTION:
                case OPTGROUP:
                case RT_OR_RP:
                    pop();
                    continue;
                default:
                    return;
            }
        }
    }

    private boolean isSecondOnStackBody() {
        return currentPtr >= 1 && stack[1].group == TreeBuilder.BODY;
    }

    private void documentModeInternal(DocumentMode m, String publicIdentifier,
            String systemIdentifier, boolean html4SpecificAdditionalErrorChecks)
            throws SAXException {
        quirks = (m == DocumentMode.QUIRKS_MODE);
        if (documentModeHandler != null) {
            documentModeHandler.documentMode(
                    m
                    // [NOCPP[
                    , publicIdentifier, systemIdentifier,
                    html4SpecificAdditionalErrorChecks
            // ]NOCPP]
            );
        }
        // [NOCPP[
        documentMode(m, publicIdentifier, systemIdentifier,
                html4SpecificAdditionalErrorChecks);
        // ]NOCPP]
    }

    private boolean isAlmostStandards(String publicIdentifier,
            String systemIdentifier) {
        if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                "-//w3c//dtd xhtml 1.0 transitional//en", publicIdentifier)) {
            return true;
        }
        if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                "-//w3c//dtd xhtml 1.0 frameset//en", publicIdentifier)) {
            return true;
        }
        if (systemIdentifier != null) {
            if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                    "-//w3c//dtd html 4.01 transitional//en", publicIdentifier)) {
                return true;
            }
            if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                    "-//w3c//dtd html 4.01 frameset//en", publicIdentifier)) {
                return true;
            }
        }
        return false;
    }

    private boolean isQuirky(@Local String name, String publicIdentifier,
            String systemIdentifier, boolean forceQuirks) {
        if (forceQuirks) {
            return true;
        }
        if (name != HTML_LOCAL) {
            return true;
        }
        if (publicIdentifier != null) {
            for (int i = 0; i < TreeBuilder.QUIRKY_PUBLIC_IDS.length; i++) {
                if (Portability.lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(
                        TreeBuilder.QUIRKY_PUBLIC_IDS[i], publicIdentifier)) {
                    return true;
                }
            }
            if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                    "-//w3o//dtd w3 html strict 3.0//en//", publicIdentifier)
                    || Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                            "-/w3c/dtd html 4.0 transitional/en",
                            publicIdentifier)
                    || Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                            "html", publicIdentifier)) {
                return true;
            }
        }
        if (systemIdentifier == null) {
            if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                    "-//w3c//dtd html 4.01 transitional//en", publicIdentifier)) {
                return true;
            } else if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                    "-//w3c//dtd html 4.01 frameset//en", publicIdentifier)) {
                return true;
            }
        } else if (Portability.lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd",
                systemIdentifier)) {
            return true;
        }
        return false;
    }

    private void closeTheCell(int eltPos) throws SAXException {
        generateImpliedEndTags();
        if (eltPos != currentPtr) {
            err("Unclosed elements.");
        }
        while (currentPtr >= eltPos) {
            pop();
        }
        clearTheListOfActiveFormattingElementsUpToTheLastMarker();
        mode = IN_ROW;
        return;
    }

    private int findLastInTableScopeTdTh() {
        for (int i = currentPtr; i > 0; i--) {
            @Local String name = stack[i].name;
            if ("td" == name || "th" == name) {
                return i;
            } else if (name == "table") {
                return TreeBuilder.NOT_FOUND_ON_STACK;
            }
        }
        return TreeBuilder.NOT_FOUND_ON_STACK;
    }

    private void clearStackBackTo(int eltPos) throws SAXException {
        while (currentPtr > eltPos) { // > not >= intentional
            pop();
        }
    }

    private void resetTheInsertionMode() {
        foreignFlag = TreeBuilder.NOT_IN_FOREIGN;
        StackNode<T> node;
        @Local String name;
        @NsUri String ns;
        for (int i = currentPtr; i >= 0; i--) {
            node = stack[i];
            name = node.name;
            ns = node.ns;
            if (i == 0) {
                if (!(contextNamespace == "http://www.w3.org/1999/xhtml" && (contextName == "td" || contextName == "th"))) {
                    name = contextName;
                    ns = contextNamespace;
                } else {
                    mode = framesetOk ? FRAMESET_OK : IN_BODY; // XXX from Hixie's email
                    return;
                }
            }
            if ("select" == name) {
                mode = IN_SELECT;
                return;
            } else if ("td" == name || "th" == name) {
                mode = IN_CELL;
                return;
            } else if ("tr" == name) {
                mode = IN_ROW;
                return;
            } else if ("tbody" == name || "thead" == name || "tfoot" == name) {
                mode = IN_TABLE_BODY;
                return;
            } else if ("caption" == name) {
                mode = IN_CAPTION;
                return;
            } else if ("colgroup" == name) {
                mode = IN_COLUMN_GROUP;
                return;
            } else if ("table" == name) {
                mode = IN_TABLE;
                return;
            } else if ("http://www.w3.org/1999/xhtml" != ns) {
                foreignFlag = TreeBuilder.IN_FOREIGN;
                mode = framesetOk ? FRAMESET_OK : IN_BODY;
                return;
            } else if ("head" == name) {
                mode = framesetOk ? FRAMESET_OK : IN_BODY; // really
                return;
            } else if ("body" == name) {
                mode = framesetOk ? FRAMESET_OK : IN_BODY;
                return;
            } else if ("frameset" == name) {
                mode = IN_FRAMESET;
                return;
            } else if ("html" == name) {
                if (headPointer == null) {
                    mode = BEFORE_HEAD;
                } else {
                    mode = AFTER_HEAD;
                }
                return;
            } else if (i == 0) {
                mode = framesetOk ? FRAMESET_OK : IN_BODY;
                return;
            }
        }
    }

    /**
     * @throws SAXException
     * 
     */
    private void implicitlyCloseP() throws SAXException {
        int eltPos = findLastInScope("p");
        if (eltPos == TreeBuilder.NOT_FOUND_ON_STACK) {
            return;
        }
        generateImpliedEndTagsExceptFor("p");
        if (eltPos != currentPtr) {
            err("Unclosed elements.");
        }
        while (currentPtr >= eltPos) {
            pop();
        }
    }

    private boolean clearLastStackSlot() {
        stack[currentPtr] = null;
        return true;
    }

    private boolean clearLastListSlot() {
        listOfActiveFormattingElements[listPtr] = null;
        return true;
    }

    @SuppressWarnings("unchecked") private void push(StackNode<T> node) throws SAXException {
        currentPtr++;
        if (currentPtr == stack.length) {
            StackNode<T>[] newStack = new StackNode[stack.length + 64];
            System.arraycopy(stack, 0, newStack, 0, stack.length);
            Portability.releaseArray(stack);
            stack = newStack;
        }
        stack[currentPtr] = node;
        elementPushed(node.ns, node.popName, node.node);
    }

    @SuppressWarnings("unchecked") private void silentPush(StackNode<T> node) throws SAXException {
        currentPtr++;
        if (currentPtr == stack.length) {
            StackNode<T>[] newStack = new StackNode[stack.length + 64];
            System.arraycopy(stack, 0, newStack, 0, stack.length);
            Portability.releaseArray(stack);
            stack = newStack;
        }
        stack[currentPtr] = node;
    }

    @SuppressWarnings("unchecked") private void append(StackNode<T> node) {
        listPtr++;
        if (listPtr == listOfActiveFormattingElements.length) {
            StackNode<T>[] newList = new StackNode[listOfActiveFormattingElements.length + 64];
            System.arraycopy(listOfActiveFormattingElements, 0, newList, 0,
                    listOfActiveFormattingElements.length);
            Portability.releaseArray(listOfActiveFormattingElements);
            listOfActiveFormattingElements = newList;
        }
        listOfActiveFormattingElements[listPtr] = node;
    }

    @Inline private void insertMarker() {
        append(null);
    }

    private void clearTheListOfActiveFormattingElementsUpToTheLastMarker() {
        while (listPtr > -1) {
            if (listOfActiveFormattingElements[listPtr] == null) {
                --listPtr;
                return;
            }
            listOfActiveFormattingElements[listPtr].release();
            --listPtr;
        }
    }

    @Inline private boolean isCurrent(@Local String name) {
        return name == stack[currentPtr].name;
    }

    private void removeFromStack(int pos) throws SAXException {
        if (currentPtr == pos) {
            pop();
        } else {
            fatal();
            stack[pos].release();
            System.arraycopy(stack, pos + 1, stack, pos, currentPtr - pos);
            assert clearLastStackSlot();
            currentPtr--;
        }
    }

    private void removeFromStack(StackNode<T> node) throws SAXException {
        if (stack[currentPtr] == node) {
            pop();
        } else {
            int pos = currentPtr - 1;
            while (pos >= 0 && stack[pos] != node) {
                pos--;
            }
            if (pos == -1) {
                // dead code?
                return;
            }
            fatal();
            node.release();
            System.arraycopy(stack, pos + 1, stack, pos, currentPtr - pos);
            currentPtr--;
        }
    }

    private void removeFromListOfActiveFormattingElements(int pos) {
        assert listOfActiveFormattingElements[pos] != null;
        listOfActiveFormattingElements[pos].release();
        if (pos == listPtr) {
            assert clearLastListSlot();
            listPtr--;
            return;
        }
        assert pos < listPtr;
        System.arraycopy(listOfActiveFormattingElements, pos + 1,
                listOfActiveFormattingElements, pos, listPtr - pos);
        assert clearLastListSlot();
        listPtr--;
    }

    private void adoptionAgencyEndTag(@Local String name) throws SAXException {
        // If you crash around here, perhaps some stack node variable claimed to
        // be a weak ref isn't.
        flushCharacters();
        for (;;) {
            int formattingEltListPos = listPtr;
            while (formattingEltListPos > -1) {
                StackNode<T> listNode = listOfActiveFormattingElements[formattingEltListPos]; // weak
                                                                                              // ref
                if (listNode == null) {
                    formattingEltListPos = -1;
                    break;
                } else if (listNode.name == name) {
                    break;
                }
                formattingEltListPos--;
            }
            if (formattingEltListPos == -1) {
                err("No element \u201C" + name + "\u201D to close.");
                return;
            }
            StackNode<T> formattingElt = listOfActiveFormattingElements[formattingEltListPos]; // this
            // *looks*
            // like
            // a
            // weak
            // ref
            // to
            // the
            // list
            // of
            // formatting
            // elements
            int formattingEltStackPos = currentPtr;
            boolean inScope = true;
            while (formattingEltStackPos > -1) {
                StackNode<T> node = stack[formattingEltStackPos]; // weak ref
                if (node == formattingElt) {
                    break;
                } else if (node.scoping) {
                    inScope = false;
                }
                formattingEltStackPos--;
            }
            if (formattingEltStackPos == -1) {
                err("No element \u201C" + name + "\u201D to close.");
                removeFromListOfActiveFormattingElements(formattingEltListPos);
                return;
            }
            if (!inScope) {
                err("No element \u201C" + name + "\u201D to close.");
                return;
            }
            // stackPos now points to the formatting element and it is in scope
            if (formattingEltStackPos != currentPtr) {
                err("End tag \u201C" + name + "\u201D violates nesting rules.");
            }
            int furthestBlockPos = formattingEltStackPos + 1;
            while (furthestBlockPos <= currentPtr) {
                StackNode<T> node = stack[furthestBlockPos]; // weak ref
                if (node.scoping || node.special) {
                    break;
                }
                furthestBlockPos++;
            }
            if (furthestBlockPos > currentPtr) {
                // no furthest block
                while (currentPtr >= formattingEltStackPos) {
                    pop();
                }
                removeFromListOfActiveFormattingElements(formattingEltListPos);
                return;
            }
            StackNode<T> commonAncestor = stack[formattingEltStackPos - 1]; // weak
            // ref
            StackNode<T> furthestBlock = stack[furthestBlockPos]; // weak ref
            // detachFromParent(furthestBlock.node); XXX AAA CHANGE
            int bookmark = formattingEltListPos;
            int nodePos = furthestBlockPos;
            StackNode<T> lastNode = furthestBlock; // weak ref
            for (;;) {
                nodePos--;
                StackNode<T> node = stack[nodePos]; // weak ref
                int nodeListPos = findInListOfActiveFormattingElements(node);
                if (nodeListPos == -1) {
                    assert formattingEltStackPos < nodePos;
                    assert bookmark < nodePos;
                    assert furthestBlockPos > nodePos;
                    removeFromStack(nodePos); // node is now a bad pointer in
                    // C++
                    furthestBlockPos--;
                    continue;
                }
                // now node is both on stack and in the list
                if (nodePos == formattingEltStackPos) {
                    break;
                }
                if (nodePos == furthestBlockPos) {
                    bookmark = nodeListPos + 1;
                }
                // if (hasChildren(node.node)) { XXX AAA CHANGE
                assert node == listOfActiveFormattingElements[nodeListPos];
                assert node == stack[nodePos];
                T clone = createElement("http://www.w3.org/1999/xhtml",
                        node.name, node.attributes.cloneAttributes(null));
                StackNode<T> newNode = new StackNode<T>(node.group, node.ns,
                        node.name, clone, node.scoping, node.special,
                        node.fosterParenting, node.popName, node.attributes); // creation
                // ownership
                // goes
                // to
                // stack
                node.dropAttributes(); // adopt ownership to newNode
                stack[nodePos] = newNode;
                newNode.retain(); // retain for list
                listOfActiveFormattingElements[nodeListPos] = newNode;
                node.release(); // release from stack
                node.release(); // release from list
                node = newNode;
                Portability.releaseElement(clone);
                // } XXX AAA CHANGE
                detachFromParent(lastNode.node);
                appendElement(lastNode.node, node.node);
                lastNode = node;
            }
            if (commonAncestor.fosterParenting) {
                fatal();
                detachFromParent(lastNode.node);
                insertIntoFosterParent(lastNode.node);
            } else {
                detachFromParent(lastNode.node);
                appendElement(lastNode.node, commonAncestor.node);
            }
            T clone = createElement("http://www.w3.org/1999/xhtml",
                    formattingElt.name,
                    formattingElt.attributes.cloneAttributes(null));
            StackNode<T> formattingClone = new StackNode<T>(
                    formattingElt.group, formattingElt.ns, formattingElt.name,
                    clone, formattingElt.scoping, formattingElt.special,
                    formattingElt.fosterParenting, formattingElt.popName,
                    formattingElt.attributes); // Ownership
            // transfers
            // to
            // stack
            // below
            formattingElt.dropAttributes(); // transfer ownership to
                                            // formattingClone
            appendChildrenToNewParent(furthestBlock.node, clone);
            appendElement(clone, furthestBlock.node);
            removeFromListOfActiveFormattingElements(formattingEltListPos);
            insertIntoListOfActiveFormattingElements(formattingClone, bookmark);
            assert formattingEltStackPos < furthestBlockPos;
            removeFromStack(formattingEltStackPos);
            // furthestBlockPos is now off by one and points to the slot after
            // it
            insertIntoStack(formattingClone, furthestBlockPos);
            Portability.releaseElement(clone);
        }
    }

    private void insertIntoStack(StackNode<T> node, int position)
            throws SAXException {
        assert currentPtr + 1 < stack.length;
        assert position <= currentPtr + 1;
        if (position == currentPtr + 1) {
            flushCharacters();
            push(node);
        } else {
            System.arraycopy(stack, position, stack, position + 1,
                    (currentPtr - position) + 1);
            currentPtr++;
            stack[position] = node;
        }
    }

    private void insertIntoListOfActiveFormattingElements(
            StackNode<T> formattingClone, int bookmark) {
        formattingClone.retain();
        assert listPtr + 1 < listOfActiveFormattingElements.length;
        if (bookmark <= listPtr) {
            System.arraycopy(listOfActiveFormattingElements, bookmark,
                    listOfActiveFormattingElements, bookmark + 1,
                    (listPtr - bookmark) + 1);
        }
        listPtr++;
        listOfActiveFormattingElements[bookmark] = formattingClone;
    }

    private int findInListOfActiveFormattingElements(StackNode<T> node) {
        for (int i = listPtr; i >= 0; i--) {
            if (node == listOfActiveFormattingElements[i]) {
                return i;
            }
        }
        return -1;
    }

    private int findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(
            @Local String name) {
        for (int i = listPtr; i >= 0; i--) {
            StackNode<T> node = listOfActiveFormattingElements[i];
            if (node == null) {
                return -1;
            } else if (node.name == name) {
                return i;
            }
        }
        return -1;
    }

    private int findLastOrRoot(@Local String name) {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].name == name) {
                return i;
            }
        }
        return 0;
    }

    private int findLastOrRoot(int group) {
        for (int i = currentPtr; i > 0; i--) {
            if (stack[i].group == group) {
                return i;
            }
        }
        return 0;
    }

    private void addAttributesToBody(HtmlAttributes attributes)
            throws SAXException {
        // [NOCPP[
        checkAttributes(attributes, "http://www.w3.org/1999/xhtml");
        // ]NOCPP]
        if (currentPtr >= 1) {
            StackNode<T> body = stack[1];
            if (body.group == TreeBuilder.BODY) {
                addAttributesToElement(body.node, attributes);
            }
        }
    }

    private void addAttributesToHtml(HtmlAttributes attributes)
            throws SAXException {
        // [NOCPP[
        checkAttributes(attributes, "http://www.w3.org/1999/xhtml");
        // ]NOCPP]
        addAttributesToElement(stack[0].node, attributes);
    }

    private void pushHeadPointerOntoStack() throws SAXException {
        assert headPointer != null;
        assert !fragment;
        assert mode == AFTER_HEAD;
        flushCharacters();
        fatal();
        silentPush(new StackNode<T>("http://www.w3.org/1999/xhtml", ElementName.HEAD,
                headPointer));
    }

    /**
     * @throws SAXException
     * 
     */
    private void reconstructTheActiveFormattingElements() throws SAXException {
        if (listPtr == -1) {
            return;
        }
        StackNode<T> mostRecent = listOfActiveFormattingElements[listPtr];
        if (mostRecent == null || isInStack(mostRecent)) {
            return;
        }
        int entryPos = listPtr;
        for (;;) {
            entryPos--;
            if (entryPos == -1) {
                break;
            }
            if (listOfActiveFormattingElements[entryPos] == null) {
                break;
            }
            if (isInStack(listOfActiveFormattingElements[entryPos])) {
                break;
            }
        }
        if (entryPos < listPtr) {
            flushCharacters();
        }
        while (entryPos < listPtr) {
            entryPos++;
            StackNode<T> entry = listOfActiveFormattingElements[entryPos];
            T clone = createElement("http://www.w3.org/1999/xhtml", entry.name,
                    entry.attributes.cloneAttributes(null));
            StackNode<T> entryClone = new StackNode<T>(entry.group, entry.ns,
                    entry.name, clone, entry.scoping, entry.special,
                    entry.fosterParenting, entry.popName, entry.attributes);
            entry.dropAttributes(); // transfer ownership to entryClone
            StackNode<T> currentNode = stack[currentPtr];
            if (currentNode.fosterParenting) {
                insertIntoFosterParent(clone);
            } else {
                appendElement(clone, currentNode.node);
            }
            push(entryClone);
            // stack takes ownership of the local variable
            listOfActiveFormattingElements[entryPos] = entryClone;
            // overwriting the old entry on the list, so release & retain
            entry.release();
            entryClone.retain();
        }
    }

    private void insertIntoFosterParent(T child) throws SAXException {
        int eltPos = findLastOrRoot(TreeBuilder.TABLE);
        StackNode<T> node = stack[eltPos];
        T elt = node.node;
        if (eltPos == 0) {
            appendElement(child, elt);
            return;
        }
        insertFosterParentedChild(child, elt, stack[eltPos - 1].node);
    }

    private boolean isInStack(StackNode<T> node) {
        for (int i = currentPtr; i >= 0; i--) {
            if (stack[i] == node) {
                return true;
            }
        }
        return false;
    }

    private void pop() throws SAXException {
        flushCharacters();
        StackNode<T> node = stack[currentPtr];
        assert clearLastStackSlot();
        currentPtr--;
        elementPopped(node.ns, node.popName, node.node);
        node.release();
    }

    private void silentPop() throws SAXException {
        flushCharacters();
        StackNode<T> node = stack[currentPtr];
        assert clearLastStackSlot();
        currentPtr--;
        node.release();
    }

    private void popOnEof() throws SAXException {
        flushCharacters();
        StackNode<T> node = stack[currentPtr];
        assert clearLastStackSlot();
        currentPtr--;
        markMalformedIfScript(node.node);
        elementPopped(node.ns, node.popName, node.node);
        node.release();
    }

    // [NOCPP[
    private void checkAttributes(HtmlAttributes attributes, @NsUri String ns)
            throws SAXException {
        if (errorHandler != null) {
            int len = attributes.getXmlnsLength();
            for (int i = 0; i < len; i++) {
                AttributeName name = attributes.getXmlnsAttributeName(i);
                if (name == AttributeName.XMLNS) {
                    if (html4) {
                        err("Attribute \u201Cxmlns\u201D not allowed here. (HTML4-only error.)");
                    } else {
                        String xmlns = attributes.getXmlnsValue(i);
                        if (!ns.equals(xmlns)) {
                            err("Bad value \u201C"
                                    + xmlns
                                    + "\u201D for the attribute \u201Cxmlns\u201D (only \u201C"
                                    + ns + "\u201D permitted here).");
                            switch (namePolicy) {
                                case ALTER_INFOSET:
                                    // fall through
                                case ALLOW:
                                    warn("Attribute \u201Cxmlns\u201D is not serializable as XML 1.0.");
                                    break;
                                case FATAL:
                                    fatal("Attribute \u201Cxmlns\u201D is not serializable as XML 1.0.");
                                    break;
                            }
                        }
                    }
                } else if (ns != "http://www.w3.org/1999/xhtml"
                        && name == AttributeName.XMLNS_XLINK) {
                    String xmlns = attributes.getXmlnsValue(i);
                    if (!"http://www.w3.org/1999/xlink".equals(xmlns)) {
                        err("Bad value \u201C"
                                + xmlns
                                + "\u201D for the attribute \u201Cxmlns:link\u201D (only \u201Chttp://www.w3.org/1999/xlink\u201D permitted here).");
                        switch (namePolicy) {
                            case ALTER_INFOSET:
                                // fall through
                            case ALLOW:
                                warn("Attribute \u201Cxmlns:xlink\u201D with the value \u201Chttp://www.w3org/1999/xlink\u201D is not serializable as XML 1.0 without changing document semantics.");
                                break;
                            case FATAL:
                                fatal("Attribute \u201Cxmlns:xlink\u201D with the value \u201Chttp://www.w3org/1999/xlink\u201D is not serializable as XML 1.0 without changing document semantics.");
                                break;
                        }
                    }
                } else {
                    err("Attribute \u201C" + attributes.getXmlnsLocalName(i)
                            + "\u201D not allowed here.");
                    switch (namePolicy) {
                        case ALTER_INFOSET:
                            // fall through
                        case ALLOW:
                            warn("Attribute with the local name \u201C"
                                    + attributes.getXmlnsLocalName(i)
                                    + "\u201D is not serializable as XML 1.0.");
                            break;
                        case FATAL:
                            fatal("Attribute with the local name \u201C"
                                    + attributes.getXmlnsLocalName(i)
                                    + "\u201D is not serializable as XML 1.0.");
                            break;
                    }
                }
            }
        }
        attributes.processNonNcNames(this, namePolicy);
    }

    private String checkPopName(@Local String name) throws SAXException {
        if (NCName.isNCName(name)) {
            return name;
        } else {
            switch (namePolicy) {
                case ALLOW:
                    warn("Element name \u201C" + name
                            + "\u201D cannot be represented as XML 1.0.");
                    return name;
                case ALTER_INFOSET:
                    warn("Element name \u201C" + name
                            + "\u201D cannot be represented as XML 1.0.");
                    return NCName.escapeName(name);
                case FATAL:
                    fatal("Element name \u201C" + name
                            + "\u201D cannot be represented as XML 1.0.");
            }
        }
        return null; // keep compiler happy
    }

    // ]NOCPP]

    private void appendHtmlElementToDocumentAndPush(HtmlAttributes attributes)
            throws SAXException {
        // [NOCPP[
        checkAttributes(attributes, "http://www.w3.org/1999/xhtml");
        // ]NOCPP]
        T elt = createHtmlElementSetAsRoot(attributes);
        StackNode<T> node = new StackNode<T>("http://www.w3.org/1999/xhtml",
                ElementName.HTML, elt);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendHtmlElementToDocumentAndPush() throws SAXException {
        appendHtmlElementToDocumentAndPush(tokenizer.emptyAttributes());
    }

    private void appendToCurrentNodeAndPushHeadElement(HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, "http://www.w3.org/1999/xhtml");
        // ]NOCPP]
        T elt = createElement("http://www.w3.org/1999/xhtml", "head",
                attributes);
        appendElement(elt, stack[currentPtr].node);
        headPointer = elt;
        Portability.retainElement(headPointer);
        StackNode<T> node = new StackNode<T>("http://www.w3.org/1999/xhtml",
                ElementName.HEAD, elt);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushBodyElement(HtmlAttributes attributes)
            throws SAXException {
        appendToCurrentNodeAndPushElement("http://www.w3.org/1999/xhtml",
                ElementName.BODY, attributes);
    }

    private void appendToCurrentNodeAndPushBodyElement() throws SAXException {
        appendToCurrentNodeAndPushBodyElement(tokenizer.emptyAttributes());
    }

    private void appendToCurrentNodeAndPushFormElementMayFoster(
            HtmlAttributes attributes) throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, "http://www.w3.org/1999/xhtml");
        // ]NOCPP]
        T elt = createElement("http://www.w3.org/1999/xhtml", "form",
                attributes);
        formPointer = elt;
        Portability.retainElement(formPointer);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        StackNode<T> node = new StackNode<T>("http://www.w3.org/1999/xhtml",
                ElementName.FORM, elt);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushFormattingElementMayFoster(
            @NsUri String ns, ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, ns);
        // ]NOCPP]
        // This method can't be called for custom elements
        T elt = createElement(ns, elementName.name, attributes);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        StackNode<T> node = new StackNode<T>(ns, elementName, elt,
                attributes.cloneAttributes(null));
        push(node);
        append(node);
        node.retain(); // append doesn't retain itself
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushElement(@NsUri String ns,
            ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, ns);
        // ]NOCPP]
        // This method can't be called for custom elements
        T elt = createElement(ns, elementName.name, attributes);
        appendElement(elt, stack[currentPtr].node);
        StackNode<T> node = new StackNode<T>(ns, elementName, elt);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushElementMayFoster(@NsUri String ns,
            ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        @Local String popName = elementName.name;
        // [NOCPP[
        checkAttributes(attributes, ns);
        if (elementName.custom) {
            popName = checkPopName(popName);
        }
        // ]NOCPP]
        T elt = createElement(ns, popName, attributes);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        StackNode<T> node = new StackNode<T>(ns, elementName, elt, popName);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushElementMayFosterNoScoping(
            @NsUri String ns, ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        @Local String popName = elementName.name;
        // [NOCPP[
        checkAttributes(attributes, ns);
        if (elementName.custom) {
            popName = checkPopName(popName);
        }
        // ]NOCPP]
        T elt = createElement(ns, popName, attributes);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        StackNode<T> node = new StackNode<T>(ns, elementName, elt, popName,
                false);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushElementMayFosterCamelCase(
            @NsUri String ns, ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        @Local String popName = elementName.camelCaseName;
        // [NOCPP[
        checkAttributes(attributes, ns);
        if (elementName.custom) {
            popName = checkPopName(popName);
        }
        // ]NOCPP]
        T elt = createElement(ns, popName, attributes);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        StackNode<T> node = new StackNode<T>(ns, elementName, elt, popName,
                ElementName.FOREIGNOBJECT == elementName);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendToCurrentNodeAndPushElementMayFoster(@NsUri String ns,
            ElementName elementName, HtmlAttributes attributes, T form)
            throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, ns);
        // ]NOCPP]
        // Can't be called for custom elements
        T elt = createElement(ns, elementName.name, attributes, form);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        StackNode<T> node = new StackNode<T>(ns, elementName, elt);
        push(node);
        Portability.releaseElement(elt);
    }

    private void appendVoidElementToCurrentMayFoster(
            @NsUri String ns, @Local String name, HtmlAttributes attributes,
            T form) throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, ns);
        // ]NOCPP]
        // Can't be called for custom elements
        T elt = createElement(ns, name, attributes, form);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        elementPushed(ns, name, elt);
        elementPopped(ns, name, elt);
        Portability.releaseElement(elt);
    }

    private void appendVoidElementToCurrentMayFoster(
            @NsUri String ns, ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        @Local String popName = elementName.name;
        // [NOCPP[
        checkAttributes(attributes, ns);
        if (elementName.custom) {
            popName = checkPopName(popName);
        }
        // ]NOCPP]
        T elt = createElement(ns, popName, attributes);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        elementPushed(ns, popName, elt);
        elementPopped(ns, popName, elt);
        Portability.releaseElement(elt);
    }

    private void appendVoidElementToCurrentMayFosterCamelCase(
            @NsUri String ns, ElementName elementName, HtmlAttributes attributes)
            throws SAXException {
        flushCharacters();
        @Local String popName = elementName.camelCaseName;
        // [NOCPP[
        checkAttributes(attributes, ns);
        if (elementName.custom) {
            popName = checkPopName(popName);
        }
        // ]NOCPP]
        T elt = createElement(ns, popName, attributes);
        StackNode<T> current = stack[currentPtr];
        if (current.fosterParenting) {
            fatal();
            insertIntoFosterParent(elt);
        } else {
            appendElement(elt, current.node);
        }
        elementPushed(ns, popName, elt);
        elementPopped(ns, popName, elt);
        Portability.releaseElement(elt);
    }

    private void appendVoidElementToCurrent(
            @NsUri String ns, @Local String name, HtmlAttributes attributes,
            T form) throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, ns);
        // ]NOCPP]
        // Can't be called for custom elements
        T elt = createElement(ns, name, attributes, form);
        StackNode<T> current = stack[currentPtr];
        appendElement(elt, current.node);
        elementPushed(ns, name, elt);
        elementPopped(ns, name, elt);
        Portability.releaseElement(elt);
    }

    private void appendVoidFormToCurrent(HtmlAttributes attributes) throws SAXException {
        flushCharacters();
        // [NOCPP[
        checkAttributes(attributes, "http://www.w3.org/1999/xhtml");
        // ]NOCPP]
        T elt = createElement("http://www.w3.org/1999/xhtml", "form",
                attributes);
        formPointer = elt;
        // ownership transferred to form pointer
        StackNode<T> current = stack[currentPtr];
        appendElement(elt, current.node);
        elementPushed("http://www.w3.org/1999/xhtml", "form", elt);
        elementPopped("http://www.w3.org/1999/xhtml", "form", elt);
    }

    protected void accumulateCharacters(@Const @NoLength char[] buf, int start,
            int length) throws SAXException {
        appendCharacters(stack[currentPtr].node, buf, start, length);
    }

    protected final void accumulateCharacter(char c) throws SAXException {
        int newLen = charBufferLen + 1;
        if (newLen > charBuffer.length) {
            char[] newBuf = new char[newLen];
            System.arraycopy(charBuffer, 0, newBuf, 0, charBufferLen);
            Portability.releaseArray(charBuffer);
            charBuffer = newBuf;
        }
        charBuffer[charBufferLen] = c;
        charBufferLen = newLen;
    }

    // ------------------------------- //

    protected final void requestSuspension() {
        tokenizer.requestSuspension();
    }

    protected abstract T createElement(@NsUri String ns, @Local String name,
            HtmlAttributes attributes) throws SAXException;

    protected T createElement(@NsUri String ns, @Local String name,
            HtmlAttributes attributes, T form) throws SAXException {
        return createElement("http://www.w3.org/1999/xhtml", name, attributes);
    }

    protected abstract T createHtmlElementSetAsRoot(HtmlAttributes attributes)
            throws SAXException;

    protected abstract void detachFromParent(T element) throws SAXException;

    protected abstract boolean hasChildren(T element) throws SAXException;

    protected abstract void appendElement(T child, T newParent)
            throws SAXException;

    protected abstract void appendChildrenToNewParent(T oldParent, T newParent)
            throws SAXException;

    protected abstract void insertFosterParentedChild(T child, T table,
            T stackParent) throws SAXException;

    protected abstract void insertFosterParentedCharacters(
            @NoLength char[] buf, int start, int length, T table, T stackParent)
            throws SAXException;

    protected abstract void appendCharacters(T parent, @NoLength char[] buf,
            int start, int length) throws SAXException;

    protected abstract void appendComment(T parent, @NoLength char[] buf,
            int start, int length) throws SAXException;

    protected abstract void appendCommentToDocument(@NoLength char[] buf,
            int start, int length) throws SAXException;

    protected abstract void addAttributesToElement(T element,
            HtmlAttributes attributes) throws SAXException;

    protected void markMalformedIfScript(T elt) throws SAXException {

    }

    protected void start(boolean fragmentMode) throws SAXException {

    }

    protected void end() throws SAXException {

    }

    protected void appendDoctypeToDocument(@Local String name,
            String publicIdentifier, String systemIdentifier)
            throws SAXException {

    }

    protected void elementPushed(@NsUri String ns, @Local String name, T node)
            throws SAXException {

    }

    protected void elementPopped(@NsUri String ns, @Local String name, T node)
            throws SAXException {

    }

    // [NOCPP[

    protected void documentMode(DocumentMode m, String publicIdentifier,
            String systemIdentifier, boolean html4SpecificAdditionalErrorChecks)
            throws SAXException {

    }

    /**
     * @see nu.validator.htmlparser.common.TokenHandler#wantsComments()
     */
    public boolean wantsComments() {
        return wantingComments;
    }

    public void setIgnoringComments(boolean ignoreComments) {
        wantingComments = !ignoreComments;
    }

    /**
     * Sets the errorHandler.
     * 
     * @param errorHandler
     *            the errorHandler to set
     */
    public final void setErrorHandler(ErrorHandler errorHandler) {
        this.errorHandler = errorHandler;
    }

    /**
     * Returns the errorHandler.
     * 
     * @return the errorHandler
     */
    public ErrorHandler getErrorHandler() {
        return errorHandler;
    }

    /**
     * The argument MUST be an interned string or <code>null</code>.
     * 
     * @param context
     */
    public final void setFragmentContext(@Local String context) {
        this.contextName = context;
        this.contextNamespace = "http://www.w3.org/1999/xhtml";
        this.contextNode = null;
        this.fragment = (contextName != null);
        this.quirks = false;
    }

    // ]NOCPP]

    /**
     * The argument MUST be an interned string or <code>null</code>.
     * 
     * @param context
     */
    public final void setFragmentContext(@Local String context,
            @NsUri String ns, T node, boolean quirks) {
        this.contextName = context;
        Portability.retainLocal(context);
        this.contextNamespace = ns;
        this.contextNode = node;
        Portability.retainElement(node);
        this.fragment = (contextName != null);
        this.quirks = quirks;
    }

    protected final T currentNode() {
        return stack[currentPtr].node;
    }

    /**
     * Returns the scriptingEnabled.
     * 
     * @return the scriptingEnabled
     */
    public boolean isScriptingEnabled() {
        return scriptingEnabled;
    }

    /**
     * Sets the scriptingEnabled.
     * 
     * @param scriptingEnabled
     *            the scriptingEnabled to set
     */
    public void setScriptingEnabled(boolean scriptingEnabled) {
        this.scriptingEnabled = scriptingEnabled;
    }

    // [NOCPP[

    /**
     * Sets the doctypeExpectation.
     * 
     * @param doctypeExpectation
     *            the doctypeExpectation to set
     */
    public void setDoctypeExpectation(DoctypeExpectation doctypeExpectation) {
        this.doctypeExpectation = doctypeExpectation;
    }

    public void setNamePolicy(XmlViolationPolicy namePolicy) {
        this.namePolicy = namePolicy;
    }

    /**
     * Sets the documentModeHandler.
     * 
     * @param documentModeHandler
     *            the documentModeHandler to set
     */
    public void setDocumentModeHandler(DocumentModeHandler documentModeHandler) {
        this.documentModeHandler = documentModeHandler;
    }

    /**
     * Sets the reportingDoctype.
     * 
     * @param reportingDoctype
     *            the reportingDoctype to set
     */
    public void setReportingDoctype(boolean reportingDoctype) {
        this.reportingDoctype = reportingDoctype;
    }

    // ]NOCPP]

    /**
     * @see nu.validator.htmlparser.common.TokenHandler#inForeign()
     */
    public boolean inForeign() throws SAXException {
        return foreignFlag == IN_FOREIGN;
    }

    /**
     * Flushes the pending characters. Public for document.write use cases only.
     * @throws SAXException
     */
    public final void flushCharacters() throws SAXException {
        if (charBufferLen > 0) {
            StackNode<T> current = stack[currentPtr];
            if (current.fosterParenting && charBufferContainsNonWhitespace()) {
                err("Misplaced non-space characters insided a table.");
                int eltPos = findLastOrRoot(TreeBuilder.TABLE);
                StackNode<T> node = stack[eltPos];
                T elt = node.node;
                if (eltPos == 0) {
                    appendCharacters(elt, charBuffer, 0, charBufferLen);
                    charBufferLen = 0;
                    return;
                }
                insertFosterParentedCharacters(charBuffer, 0, charBufferLen,
                        elt, stack[eltPos - 1].node);
                charBufferLen = 0;
                return;
            }
            appendCharacters(currentNode(), charBuffer, 0, charBufferLen);
            charBufferLen = 0;
        }
    }

    private boolean charBufferContainsNonWhitespace() {
        for (int i = 0; i < charBufferLen; i++) {
            switch (charBuffer[i]) {
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                case '\u000C':
                    continue;
                default:
                    return true;
            }
        }
        return false;
    }

    /**
     * Creates a comparable snapshot of the tree builder state. Snapshot
     * creation is only supported immediately after a script end tag has been
     * processed. In C++ the caller is responsible for calling
     * <code>delete</code> on the returned object.
     * 
     * @return a snapshot.
     * @throws SAXException
     */
    @SuppressWarnings("unchecked") public TreeBuilderState<T> newSnapshot()
            throws SAXException {
        StackNode<T>[] listCopy = new StackNode[listPtr + 1];
        for (int i = 0; i < listCopy.length; i++) {
            StackNode<T> node = listOfActiveFormattingElements[i];
            if (node != null) {
                StackNode<T> newNode = new StackNode<T>(node.group, node.ns,
                        node.name, node.node, node.scoping, node.special,
                        node.fosterParenting, node.popName,
                        node.attributes.cloneAttributes(null));
                listCopy[i] = newNode;
            } else {
                listCopy[i] = null;
            }
        }
        StackNode<T>[] stackCopy = new StackNode[currentPtr + 1];
        for (int i = 0; i < stackCopy.length; i++) {
            StackNode<T> node = stack[i];
            int listIndex = findInListOfActiveFormattingElements(node);
            if (listIndex == -1) {
                StackNode<T> newNode = new StackNode<T>(node.group, node.ns,
                        node.name, node.node, node.scoping, node.special,
                        node.fosterParenting, node.popName,
                        null);
                stackCopy[i] = newNode;
            } else {
                stackCopy[i] = listCopy[listIndex];
                stackCopy[i].retain();
            }
        }
        Portability.retainElement(formPointer);
        return new StateSnapshot<T>(stackCopy, listCopy, formPointer, headPointer, mode, originalMode, framesetOk, foreignFlag, needToDropLF, quirks);
    }

    public boolean snapshotMatches(TreeBuilderState<T> snapshot) {
        StackNode<T>[] stackCopy = snapshot.getStack();
        int stackLen = snapshot.getStackLength();
        StackNode<T>[] listCopy = snapshot.getListOfActiveFormattingElements();
        int listLen = snapshot.getListLength();

        if (stackLen != currentPtr + 1
                || listLen != listPtr + 1
                || formPointer != snapshot.getFormPointer()
                || headPointer != snapshot.getHeadPointer()
                || mode != snapshot.getMode()
                || originalMode != snapshot.getOriginalMode()
                || framesetOk != snapshot.isFramesetOk()
                || foreignFlag != snapshot.getForeignFlag()
                || needToDropLF != snapshot.isNeedToDropLF()
                || quirks != snapshot.isQuirks()) { // maybe just assert quirks
            return false;
        }
        for (int i = listLen - 1; i >= 0; i--) {
            if (listCopy[i] == null
                    && listOfActiveFormattingElements[i] == null) {
                continue;
            } else if (listCopy[i] == null
                    || listOfActiveFormattingElements[i] == null) {
                return false;
            }
            if (listCopy[i].node != listOfActiveFormattingElements[i].node) {
                return false; // it's possible that this condition is overly
                              // strict
            }
        }
        for (int i = stackLen - 1; i >= 0; i--) {
            if (stackCopy[i].node != stack[i].node) {
                return false;
            }
        }
        return true;
    }

    @SuppressWarnings("unchecked") public void loadState(
            TreeBuilderState<T> snapshot, Interner interner)
            throws SAXException {
        StackNode<T>[] stackCopy = snapshot.getStack();
        int stackLen = snapshot.getStackLength();
        StackNode<T>[] listCopy = snapshot.getListOfActiveFormattingElements();
        int listLen = snapshot.getListLength();
        
        for (int i = 0; i <= listPtr; i++) {
            if (listOfActiveFormattingElements[i] != null) {
                listOfActiveFormattingElements[i].release();
            }
        }
        if (listOfActiveFormattingElements.length < listLen) {
            Portability.releaseArray(listOfActiveFormattingElements);
            listOfActiveFormattingElements = new StackNode[listLen];
        }
        listPtr = listLen - 1;

        for (int i = 0; i <= currentPtr; i++) {
            stack[i].release();
        }
        if (stack.length < stackLen) {
            Portability.releaseArray(stack);
            stack = new StackNode[stackLen];
        }
        currentPtr = stackLen - 1;

        for (int i = 0; i < listLen; i++) {
            StackNode<T> node = listCopy[i];
            if (node != null) {
                StackNode<T> newNode = new StackNode<T>(node.group, node.ns,
                        Portability.newLocalFromLocal(node.name, interner), node.node,
                        node.scoping, node.special, node.fosterParenting,
                        Portability.newLocalFromLocal(node.popName, interner),
                        node.attributes.cloneAttributes(null));
                listOfActiveFormattingElements[i] = newNode;
            } else {
                listOfActiveFormattingElements[i] = null;
            }
        }
        for (int i = 0; i < stackLen; i++) {
            StackNode<T> node = stackCopy[i];
            int listIndex = findInArray(node, listCopy);
            if (listIndex == -1) {
                StackNode<T> newNode = new StackNode<T>(node.group, node.ns,
                        Portability.newLocalFromLocal(node.name, interner), node.node,
                        node.scoping, node.special, node.fosterParenting,
                        Portability.newLocalFromLocal(node.popName, interner),
                        null);
                stack[i] = newNode;
            } else {
                stack[i] = listOfActiveFormattingElements[listIndex];
                stack[i].retain();
            }
        }
        Portability.releaseElement(formPointer);
        formPointer = snapshot.getFormPointer();
        Portability.retainElement(formPointer);
        Portability.releaseElement(headPointer);
        headPointer = snapshot.getHeadPointer();
        Portability.retainElement(headPointer);
        mode = snapshot.getMode();
        originalMode = snapshot.getOriginalMode();
        framesetOk = snapshot.isFramesetOk();
        foreignFlag = snapshot.getForeignFlag();
        needToDropLF = snapshot.isNeedToDropLF();
        quirks = snapshot.isQuirks();
    }

    private int findInArray(StackNode<T> node, StackNode<T>[] arr) {
        for (int i = listPtr; i >= 0; i--) {
            if (node == arr[i]) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getFormPointer()
     */
    public T getFormPointer() {
        return formPointer;
    }

    /**
     * Returns the headPointer.
     * 
     * @return the headPointer
     */
    public T getHeadPointer() {
        return headPointer;
    }
    
    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getListOfActiveFormattingElements()
     */
    public StackNode<T>[] getListOfActiveFormattingElements() {
        return listOfActiveFormattingElements;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getStack()
     */
    public StackNode<T>[] getStack() {
        return stack;
    }

    /**
     * Returns the mode.
     * 
     * @return the mode
     */
    public int getMode() {
        return mode;
    }

    /**
     * Returns the originalMode.
     * 
     * @return the originalMode
     */
    public int getOriginalMode() {
        return originalMode;
    }

    /**
     * Returns the framesetOk.
     * 
     * @return the framesetOk
     */
    public boolean isFramesetOk() {
        return framesetOk;
    }
    
    /**
     * Returns the foreignFlag.
     * 
     * @return the foreignFlag
     */
    public int getForeignFlag() {
        return foreignFlag;
    }

    /**
     * Returns the needToDropLF.
     * 
     * @return the needToDropLF
     */
    public boolean isNeedToDropLF() {
        return needToDropLF;
    }

    /**
     * Returns the quirks.
     * 
     * @return the quirks
     */
    public boolean isQuirks() {
        return quirks;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getListLength()
     */
    public int getListLength() {
        return listPtr + 1;
    }

    /**
     * @see nu.validator.htmlparser.impl.TreeBuilderState#getStackLength()
     */
    public int getStackLength() {
        return currentPtr + 1;
    }

}
