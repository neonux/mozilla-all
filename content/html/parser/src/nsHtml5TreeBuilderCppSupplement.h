/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsContentErrors.h"
#include "nsContentCreatorFunctions.h"
#include "nsIDOMDocumentType.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsEvent.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsNodeUtils.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsTraceRefcnt.h"
#include "mozAutoDocUpdate.h"

#define NS_HTML5_TREE_BUILDER_MAX_QUEUE_TIME 3000000UL // microseconds
#define NS_HTML5_TREE_BUILDER_DEFAULT_QUEUE_LENGTH 200
#define NS_HTML5_TREE_BUILDER_MIN_QUEUE_LENGTH 100
#define NS_HTML5_TREE_BUILDER_MAX_TIME_WITHOUT_FLUSH 5000 // milliseconds

// this really should be autogenerated...
jArray<PRUnichar,PRInt32> nsHtml5TreeBuilder::ISINDEX_PROMPT = jArray<PRUnichar,PRInt32>();

nsHtml5TreeBuilder::nsHtml5TreeBuilder(nsHtml5Parser* aParser)
  : documentModeHandler(aParser),
    fragment(PR_FALSE),
    formPointer(nsnull),
    headPointer(nsnull),
    mNeedsFlush(PR_FALSE),
#ifdef DEBUG
    mActive(PR_FALSE),
#endif
    mFlushTimer(do_CreateInstance("@mozilla.org/timer;1")),
    mParser(aParser)
{
  MOZ_COUNT_CTOR(nsHtml5TreeBuilder);
}

nsHtml5TreeBuilder::~nsHtml5TreeBuilder()
{
  MOZ_COUNT_DTOR(nsHtml5TreeBuilder);
  NS_ASSERTION(!mActive, "nsHtml5TreeBuilder deleted without ever calling end() on it!");
  mOpQueue.Clear();
  mFlushTimer->Cancel(); // XXX why is this even necessary? it is, though.
}

nsIContent*
nsHtml5TreeBuilder::createElement(PRInt32 aNamespace, nsIAtom* aName, nsHtml5HtmlAttributes* aAttributes)
{
  // XXX recheck http://mxr.mozilla.org/mozilla-central/source/content/base/src/nsDocument.cpp#6660
  nsIContent* newContent;
  nsCOMPtr<nsINodeInfo> nodeInfo = mParser->GetNodeInfoManager()->GetNodeInfo(aName, nsnull, aNamespace);
  NS_ASSERTION(nodeInfo, "Got null nodeinfo.");
  NS_NewElement(&newContent, nodeInfo->NamespaceID(), nodeInfo, PR_TRUE);
  NS_ASSERTION(newContent, "Element creation created null pointer.");
  PRInt32 len = aAttributes->getLength();
  for (PRInt32 i = 0; i < len; ++i) {
    newContent->SetAttr(aAttributes->getURI(i), aAttributes->getLocalName(i), aAttributes->getPrefix(i), *(aAttributes->getValue(i)), PR_FALSE);
    // XXX what to do with nsresult?
  }
  
  
  if (aNamespace != kNameSpaceID_MathML && (aName == nsHtml5Atoms::style || (aNamespace == kNameSpaceID_XHTML && aName == nsHtml5Atoms::link))) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(newContent));
    if (ssle) {
      ssle->InitStyleLinkElement(PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
#if 0
      if (!aNodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML)) {
        ssle->SetLineNumber(aLineNumber);
      }
#endif
    }
  } 
  
  return newContent;
}

nsIContent*
nsHtml5TreeBuilder::createElement(PRInt32 aNamespace, nsIAtom* aName, nsHtml5HtmlAttributes* aAttributes, nsIContent* aFormElement)
{
  nsIContent* content = createElement(aNamespace, aName, aAttributes);
  if (aFormElement) {
    nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
    NS_ASSERTION(formControl, "Form-associated element did not implement nsIFormControl.");
    nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(aFormElement));
    NS_ASSERTION(formElement, "The form element doesn't implement nsIDOMHTMLFormElement.");
    if (formControl) { // avoid crashing on <output>
      formControl->SetForm(formElement);
    }
  }
  return content; 
}

nsIContent*
nsHtml5TreeBuilder::createHtmlElementSetAsRoot(nsHtml5HtmlAttributes* aAttributes)
{
  nsIContent* content = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::html, aAttributes);
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpAppendToDocument, content);
  return content;
}

void
nsHtml5TreeBuilder::detachFromParent(nsIContent* aElement)
{
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpDetach, aElement);
}

nsIContent*
nsHtml5TreeBuilder::shallowClone(nsIContent* aElement)
{
  nsINode* clone;
  aElement->Clone(aElement->NodeInfo(), &clone);
  // XXX nsresult
  return static_cast<nsIContent*>(clone);
}

void
nsHtml5TreeBuilder::appendElement(nsIContent* aChild, nsIContent* aParent)
{
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(aChild, aParent);
}

void
nsHtml5TreeBuilder::appendChildrenToNewParent(nsIContent* aOldParent, nsIContent* aNewParent)
{
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpAppendChildrenToNewParent, aOldParent, aNewParent);
}

void
nsHtml5TreeBuilder::insertFosterParentedCharacters(PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength, nsIContent* aTable, nsIContent* aStackParent)
{
  nsCOMPtr<nsIContent> text;
  NS_NewTextNode(getter_AddRefs(text), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  text->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpFosterParent, text, aStackParent, aTable);
}

void
nsHtml5TreeBuilder::insertFosterParentedChild(nsIContent* aChild, nsIContent* aTable, nsIContent* aStackParent)
{
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpFosterParent, aChild, aStackParent, aTable);
}

void
nsHtml5TreeBuilder::appendCharacters(nsIContent* aParent, PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength)
{
  nsCOMPtr<nsIContent> text;
  NS_NewTextNode(getter_AddRefs(text), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  text->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(text, aParent);
}

void
nsHtml5TreeBuilder::appendComment(nsIContent* aParent, PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength)
{
  nsCOMPtr<nsIContent> comment;
  NS_NewCommentNode(getter_AddRefs(comment), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  comment->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(comment, aParent);
}

void
nsHtml5TreeBuilder::appendCommentToDocument(PRUnichar* aBuffer, PRInt32 aStart, PRInt32 aLength)
{
  nsCOMPtr<nsIContent> comment;
  NS_NewCommentNode(getter_AddRefs(comment), mParser->GetNodeInfoManager());
  // XXX nsresult and comment null check?
  comment->SetText(aBuffer + aStart, aLength, PR_FALSE);
  // XXX nsresult
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpAppendToDocument, comment);
}

void
nsHtml5TreeBuilder::addAttributesToElement(nsIContent* aElement, nsHtml5HtmlAttributes* aAttributes)
{
  nsIContent* holder = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::div, aAttributes);
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpAddAttributes, holder, aElement);
}

void
nsHtml5TreeBuilder::start(PRBool fragment)
{
  // XXX check that timer creation didn't fail in constructor
  if (fragment) {
    mHasProcessedBase = PR_TRUE;  
  } else {
    mHasProcessedBase = PR_FALSE;
    mParser->WillBuildModelImpl();
    mParser->GetDocument()->BeginLoad(); // XXX fragment?
  }
  mNeedsFlush = PR_FALSE;
#ifdef DEBUG
  mActive = PR_TRUE;
#endif
}

void
nsHtml5TreeBuilder::end()
{
  mFlushTimer->Cancel();
  Flush();
  mParser->ReadyToCallDidBuildModelHtml5();
#ifdef DEBUG
  mActive = PR_FALSE;
#endif
#ifdef DEBUG_hsivonen
  printf("MAX INSERTION BATCH LEN: %d\n", sInsertionBatchMaxLength);
  printf("MAX NOTIFICATION BATCH LEN: %d\n", sAppendBatchMaxSize);
  if (sAppendBatchExaminations != 0) {
    printf("AVERAGE SLOTS EXAMINED: %d\n", sAppendBatchSlotsExamined / sAppendBatchExaminations);
  }
#endif
}

void
nsHtml5TreeBuilder::appendDoctypeToDocument(nsIAtom* aName, nsString* aPublicId, nsString* aSystemId)
{
  // Adapted from nsXMLContentSink
  
  // Create a new doctype node
  nsCOMPtr<nsIDOMDocumentType> docType;
  nsAutoString voidString;
  voidString.SetIsVoid(PR_TRUE);
  NS_NewDOMDocumentType(getter_AddRefs(docType), mParser->GetNodeInfoManager(), nsnull,
                             aName, nsnull, nsnull, *aPublicId, *aSystemId,
                             voidString);
//  if (NS_FAILED(rv) || !docType) {
//    return rv;
//  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(docType);
  NS_ASSERTION(content, "doctype isn't content?");

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  // XXX if null, OOM!
  treeOp->Init(eTreeOpAppendToDocument, content);

  // nsXMLContentSink can flush here, but what's the point?
  // It can also interrupt here, but we can't.
}

void
nsHtml5TreeBuilder::elementPushed(PRInt32 aNamespace, nsIAtom* aName, nsIContent* aElement)
{
  NS_ASSERTION((aNamespace == kNameSpaceID_XHTML || aNamespace == kNameSpaceID_SVG || aNamespace == kNameSpaceID_MathML), "Element isn't HTML, SVG or MathML!");
  NS_ASSERTION(aName, "Element doesn't have local name!");
  NS_ASSERTION(aElement, "No element!");
  // Give autoloading links a chance to fire
  if (aNamespace == kNameSpaceID_XHTML) {
    if (aName == nsHtml5Atoms::body) {
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
      // XXX if null, OOM!
      treeOp->Init(eTreeOpStartLayout, nsnull);
    } else if (aName == nsHtml5Atoms::html) {
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
      // XXX if null, OOM!
      treeOp->Init(eTreeOpProcessOfflineManifest, aElement);
      return;
    }

  }
  #if 0
    else {
    nsIDocShell* docShell = mParser->GetDocShell();
    if (docShell) {
      nsresult rv = aElement->MaybeTriggerAutoLink(docShell);
      if (rv == NS_XML_AUTOLINK_REPLACE ||
          rv == NS_XML_AUTOLINK_UNDEFINED) {
        // If we do not terminate the parse, we just keep generating link trigger
        // events. We want to parse only up to the first replace link, and stop.
        mParser->Terminate();
      }
    }
  }
  #endif
  MaybeSuspend();  
}

void
nsHtml5TreeBuilder::elementPopped(PRInt32 aNamespace, nsIAtom* aName, nsIContent* aElement)
{
  NS_ASSERTION((aNamespace == kNameSpaceID_XHTML || aNamespace == kNameSpaceID_SVG || aNamespace == kNameSpaceID_MathML), "Element isn't HTML, SVG or MathML!");
  NS_ASSERTION(aName, "Element doesn't have local name!");
  NS_ASSERTION(aElement, "No element!");

  MaybeSuspend();  
  
  if (aNamespace == kNameSpaceID_MathML) {
    return;
  }  
  // we now have only SVG and HTML
  
  if (aName == nsHtml5Atoms::script) {
//    mConstrainSize = PR_TRUE; // XXX what is this?
    requestSuspension();
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpScriptEnd, aElement);
    Flush();
    return;
  }
  
  if (aName == nsHtml5Atoms::title) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpDoneAddingChildren, aElement);
    return;
  }
  
  if (aName == nsHtml5Atoms::style || (aNamespace == kNameSpaceID_XHTML && aName == nsHtml5Atoms::link)) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpUpdateStyleSheet, aElement);
    return;
  }


  if (aNamespace == kNameSpaceID_SVG) {
#if 0
    if (aElement->HasAttr(kNameSpaceID_None, nsHtml5Atoms::onload)) {
      Flush();

      nsEvent event(PR_TRUE, NS_SVG_LOAD);
      event.eventStructType = NS_SVG_EVENT;
      event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

      // Do we care about forcing presshell creation if it hasn't happened yet?
      // That is, should this code flush or something?  Does it really matter?
      // For that matter, do we really want to try getting the prescontext?  Does
      // this event ever want one?
      nsRefPtr<nsPresContext> ctx;
      nsCOMPtr<nsIPresShell> shell = mParser->GetDocument()->GetPrimaryShell();
      if (shell) {
        ctx = shell->GetPresContext();
      }
      nsEventDispatcher::Dispatch(aElement, ctx, &event);
    }
#endif
    return;
  }  
  // we now have only HTML

  // Some HTML nodes need DoneAddingChildren() called to initialize
  // properly (eg form state restoration).
  // XXX expose ElementName group here and do switch
  if (aName == nsHtml5Atoms::select ||
        aName == nsHtml5Atoms::textarea ||
#ifdef MOZ_MEDIA
        aName == nsHtml5Atoms::video ||
        aName == nsHtml5Atoms::audio ||
#endif
        aName == nsHtml5Atoms::object ||
        aName == nsHtml5Atoms::applet) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpDoneAddingChildren, aElement);
    return;
  }

  if (aName == nsHtml5Atoms::input ||
      aName == nsHtml5Atoms::button) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpDoneCreatingElement, aElement);
    return;
  }
  
  if (aName == nsHtml5Atoms::base) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpProcessBase, aElement);
    return;
  }
  
  if (aName == nsHtml5Atoms::meta) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    // XXX if null, OOM!
    treeOp->Init(eTreeOpProcessMeta, aElement);
    return;
  }

  return;
}

void
nsHtml5TreeBuilder::accumulateCharacters(PRUnichar* aBuf, PRInt32 aStart, PRInt32 aLength)
{
  PRInt32 newFillLen = charBufferLen + aLength;
  if (newFillLen > charBuffer.length) {
    PRInt32 newAllocLength = newFillLen + (newFillLen >> 1);
    jArray<PRUnichar,PRInt32> newBuf(newAllocLength);
    memcpy(newBuf, charBuffer, sizeof(PRUnichar) * charBufferLen);
    charBuffer.release();
    charBuffer = newBuf;
  }
  memcpy(charBuffer + charBufferLen, aBuf + aStart, sizeof(PRUnichar) * aLength);
  charBufferLen = newFillLen;
}

static void
TimerCallbackFunc(nsITimer* aTimer, void* aClosure)
{
  (static_cast<nsHtml5TreeBuilder*> (aClosure))->DeferredTimerFlush();
}

void
nsHtml5TreeBuilder::Flush()
{
  mNeedsFlush = PR_FALSE;
  MOZ_AUTO_DOC_UPDATE(mParser->GetDocument(), UPDATE_CONTENT_MODEL, PR_TRUE);
  
  PRTime flushStart = 0;
  
  PRUint32 opQueueLength = mOpQueue.Length();
  
  if (opQueueLength > NS_HTML5_TREE_BUILDER_MIN_QUEUE_LENGTH) { // avoid computing averages with too few ops
    flushStart = PR_Now();
  }
  
  mElementsSeenInThisAppendBatch.SetCapacity(opQueueLength * 2);
  // XXX alloc failure
  const nsHtml5TreeOperation* start = mOpQueue.Elements();
  const nsHtml5TreeOperation* end = start + opQueueLength;
  for (nsHtml5TreeOperation* iter = (nsHtml5TreeOperation*)start; iter < end; ++iter) {
    iter->Perform(this);
  }
  FlushPendingAppendNotifications();
#ifdef DEBUG_hsivonen
  if (mOpQueue.Length() > sInsertionBatchMaxLength) {
    sInsertionBatchMaxLength = opQueueLength;
  }
#endif
  mOpQueue.Clear();
  
  if (flushStart) {
    sTreeOpQueueMaxLength = (PRUint32)((NS_HTML5_TREE_BUILDER_MAX_QUEUE_TIME * (PRUint64)opQueueLength) / (PR_Now() - flushStart));
    if (sTreeOpQueueMaxLength < NS_HTML5_TREE_BUILDER_MIN_QUEUE_LENGTH) {
      sTreeOpQueueMaxLength = NS_HTML5_TREE_BUILDER_MIN_QUEUE_LENGTH;
    }
#ifdef DEBUG_hsivonen
    printf("QUEUE MAX LENGTH: %d\n", sTreeOpQueueMaxLength);
#endif    
  }
  
  mFlushTimer->InitWithFuncCallback(TimerCallbackFunc, static_cast<void*> (this), NS_HTML5_TREE_BUILDER_MAX_TIME_WITHOUT_FLUSH, nsITimer::TYPE_ONE_SHOT);
}

#ifdef DEBUG_hsivonen
PRUint32 nsHtml5TreeBuilder::sInsertionBatchMaxLength = 0;
PRUint32 nsHtml5TreeBuilder::sAppendBatchMaxSize = 0;
PRUint32 nsHtml5TreeBuilder::sAppendBatchSlotsExamined = 0;
PRUint32 nsHtml5TreeBuilder::sAppendBatchExaminations = 0;
#endif
PRUint32 nsHtml5TreeBuilder::sTreeOpQueueMaxLength = NS_HTML5_TREE_BUILDER_DEFAULT_QUEUE_LENGTH;

