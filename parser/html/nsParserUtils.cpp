/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Robert Sayre.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsScriptLoader.h"
#include "nsEscape.h"
#include "nsIParser.h"
#include "nsIDTD.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsContentUtils.h"
#include "nsIContentSink.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIFragmentContentSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsAttrName.h"
#include "nsHTMLParts.h"
#include "nsContentCID.h"
#include "nsIScriptableUnescapeHTML.h"
#include "nsParserUtils.h"
#include "nsAutoPtr.h"
#include "nsTreeSanitizer.h"
#include "nsHtml5Module.h"

#define XHTML_DIV_TAG "div xmlns=\"http://www.w3.org/1999/xhtml\""

NS_IMPL_ISUPPORTS2(nsParserUtils,
                   nsIScriptableUnescapeHTML,
                   nsIParserUtils)

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);



NS_IMETHODIMP
nsParserUtils::ConvertToPlainText(const nsAString& aFromStr,
                                  PRUint32 aFlags,
                                  PRUint32 aWrapCol,
                                  nsAString& aToStr)
{
  return nsContentUtils::ConvertToPlainText(aFromStr,
    aToStr,
    aFlags,
    aWrapCol);
}

NS_IMETHODIMP
nsParserUtils::Unescape(const nsAString& aFromStr,
                        nsAString& aToStr)
{
  return nsContentUtils::ConvertToPlainText(aFromStr,
    aToStr,
    nsIDocumentEncoder::OutputSelectionOnly |
    nsIDocumentEncoder::OutputAbsoluteLinks,
    0);
}

NS_IMETHODIMP
nsParserUtils::Sanitize(const nsAString& aFromStr,
                        PRUint32 aFlags,
                        nsAString& aToStr)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank");
  nsCOMPtr<nsIPrincipal> principal =
    do_CreateInstance("@mozilla.org/nullprincipal;1");
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsresult rv = nsContentUtils::CreateDocument(EmptyString(),
                                               EmptyString(),
                                               nsnull,
                                               uri,
                                               uri,
                                               principal,
                                               nsnull,
                                               DocumentFlavorHTML,
                                               getter_AddRefs(domDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  rv = nsContentUtils::ParseDocumentHTML(aFromStr, document, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTreeSanitizer sanitizer(aFlags);
  sanitizer.Sanitize(document);

  nsCOMPtr<nsIDocumentEncoder> encoder =
    do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html");

  encoder->NativeInit(document,
                      NS_LITERAL_STRING("text/html"),
                      nsIDocumentEncoder::OutputDontRewriteEncodingDeclaration |
                      nsIDocumentEncoder::OutputNoScriptContent |
                      nsIDocumentEncoder::OutputEncodeBasicEntities |
                      nsIDocumentEncoder::OutputLFLineBreak |
                      nsIDocumentEncoder::OutputRaw);

  return encoder->EncodeToString(aToStr);
}

NS_IMETHODIMP
nsParserUtils::ParseFragment(const nsAString& aFragment,
                             bool aIsXML,
                             nsIURI* aBaseURI,
                             nsIDOMElement* aContextElement,
                             nsIDOMDocumentFragment** aReturn)
{
  return nsParserUtils::ParseFragment(aFragment,
                                      0,
                                      aIsXML,
                                      aBaseURI,
                                      aContextElement,
                                      aReturn);
}

NS_IMETHODIMP
nsParserUtils::ParseFragment(const nsAString& aFragment,
                             PRUint32 aFlags,
                             bool aIsXML,
                             nsIURI* aBaseURI,
                             nsIDOMElement* aContextElement,
                             nsIDOMDocumentFragment** aReturn)
{
  NS_ENSURE_ARG(aContextElement);
  *aReturn = nsnull;

  nsresult rv;
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsCOMPtr<nsIDOMNode> contextNode;
  contextNode = do_QueryInterface(aContextElement);
  contextNode->GetOwnerDocument(getter_AddRefs(domDocument));
  document = do_QueryInterface(domDocument);
  NS_ENSURE_TRUE(document, NS_ERROR_NOT_AVAILABLE);

  // stop scripts
  nsRefPtr<nsScriptLoader> loader;
  bool scripts_enabled = false;
  if (document) {
    loader = document->ScriptLoader();
    scripts_enabled = loader->GetEnabled();
  }
  if (scripts_enabled) {
    loader->SetEnabled(false);
  }

  // Wrap things in a div or body for parsing, but it won't show up in
  // the fragment.
  nsAutoTArray<nsString, 2> tagStack;
  nsCAutoString base, spec;
  if (aIsXML) {
    // XHTML
    if (aBaseURI) {
      base.Append(NS_LITERAL_CSTRING(XHTML_DIV_TAG));
      base.Append(NS_LITERAL_CSTRING(" xml:base=\""));
      aBaseURI->GetSpec(spec);
      // nsEscapeHTML is good enough, because we only need to get
      // quotes, ampersands, and angle brackets
      char* escapedSpec = nsEscapeHTML(spec.get());
      if (escapedSpec)
        base += escapedSpec;
      NS_Free(escapedSpec);
      base.Append(NS_LITERAL_CSTRING("\""));
      tagStack.AppendElement(NS_ConvertUTF8toUTF16(base));
    }  else {
      tagStack.AppendElement(NS_LITERAL_STRING(XHTML_DIV_TAG));
    }
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIContent> fragment;
    if (aIsXML) {
      rv = nsContentUtils::ParseFragmentXML(aFragment,
                                            document,
                                            tagStack,
                                            true,
                                            aReturn);
      fragment = do_QueryInterface(*aReturn);
    } else {
      NS_NewDocumentFragment(aReturn,
                             document->NodeInfoManager());
      fragment = do_QueryInterface(*aReturn);
      rv = nsContentUtils::ParseFragmentHTML(aFragment,
                                             fragment,
                                             nsGkAtoms::body,
                                             kNameSpaceID_XHTML,
                                             false,
                                             true);
      // Now, set the base URI on all subtree roots.
      if (aBaseURI) {
        aBaseURI->GetSpec(spec);
        nsAutoString spec16;
        CopyUTF8toUTF16(spec, spec16);
        nsIContent* node = fragment->GetFirstChild();
        while (node) {
          if (node->IsElement()) {
            node->SetAttr(kNameSpaceID_XML,
                          nsGkAtoms::base,
                          nsGkAtoms::xml,
                          spec16,
                          false);
          }
          node = node->GetNextSibling();
        }
      }
    }
    if (fragment) {
      nsTreeSanitizer sanitizer(aFlags);
      sanitizer.Sanitize(fragment);
    }
  }

  if (scripts_enabled)
      loader->SetEnabled(true);
  
  return rv;
}
