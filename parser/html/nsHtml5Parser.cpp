/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
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

#include "nsCompatibility.h"
#include "nsScriptLoader.h"
#include "nsNetUtil.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICharsetAlias.h"
#include "nsIWebShellServices.h"
#include "nsIDocShell.h"
#include "nsEncoderDecoderUtils.h"
#include "nsContentUtils.h"
#include "nsICharsetDetector.h"
#include "nsIScriptElement.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContentViewer.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5Parser.h"
#include "nsHtml5AtomTable.h"

//-------------- Begin ParseContinue Event Definition ------------------------
/*
The parser can be explicitly interrupted by calling Suspend(). This will cause
the parser to stop processing and allow the application to return to the event
loop. The parser will schedule a nsHtml5ParserContinueEvent which will call
the parser to process the remaining data after returning to the event loop.
*/
class nsHtml5ParserContinueEvent : public nsRunnable
{
public:
  nsRefPtr<nsHtml5Parser> mParser;
  nsHtml5ParserContinueEvent(nsHtml5Parser* aParser)
    : mParser(aParser)
  {}
  NS_IMETHODIMP Run()
  {
    mParser->HandleParserContinueEvent(this);
    return NS_OK;
  }
};
//-------------- End ParseContinue Event Definition ------------------------


NS_INTERFACE_TABLE_HEAD(nsHtml5Parser)
  NS_INTERFACE_TABLE2(nsHtml5Parser, nsIParser, nsISupportsWeakReference)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHtml5Parser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHtml5Parser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mExecutor, nsIContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mStreamParser, nsIStreamListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mExecutor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStreamParser)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsHtml5Parser::nsHtml5Parser()
  : mFirstBuffer(new nsHtml5UTF16Buffer(0))
  , mLastBuffer(mFirstBuffer)
  , mExecutor(new nsHtml5TreeOpExecutor())
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder))
{
  mAtomTable.Init(); // we aren't checking for OOM anyway...
  mTokenizer->setInterner(&mAtomTable);
  // There's a zeroing operator new for everything else
}

nsHtml5Parser::~nsHtml5Parser()
{
  mTokenizer->end();
  mFirstBuffer = nsnull;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetContentSink(nsIContentSink* aSink)
{
  NS_ASSERTION(aSink == static_cast<nsIContentSink*> (mExecutor), 
               "Attempt to set a foreign sink.");
}

NS_IMETHODIMP_(nsIContentSink*)
nsHtml5Parser::GetContentSink(void)
{
  return static_cast<nsIContentSink*> (mExecutor);
}

NS_IMETHODIMP_(void)
nsHtml5Parser::GetCommand(nsCString& aCommand)
{
  aCommand.Assign("view");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetCommand(const char* aCommand)
{
  NS_ASSERTION(!strcmp(aCommand, "view"), "Parser command was not view");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetCommand(eParserCommands aParserCommand)
{
  NS_ASSERTION(aParserCommand == eViewNormal, 
               "Parser command was not eViewNormal.");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetDocumentCharset(const nsACString& aCharset, PRInt32 aCharsetSource)
{
  NS_PRECONDITION(!mExecutor->HasStarted(),
                  "Document charset set too late.");
  NS_PRECONDITION(mStreamParser, "Tried to set charset on a script-only parser.");
  mStreamParser->SetDocumentCharset(aCharset, aCharsetSource);
  mExecutor->SetDocumentCharset((nsACString&)aCharset);
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetParserFilter(nsIParserFilter* aFilter)
{
  NS_ERROR("Attempt to set a parser filter on HTML5 parser.");
}

NS_IMETHODIMP
nsHtml5Parser::GetChannel(nsIChannel** aChannel)
{
  if (mStreamParser) {
    return mStreamParser->GetChannel(aChannel);
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }
}

NS_IMETHODIMP
nsHtml5Parser::GetDTD(nsIDTD** aDTD)
{
  *aDTD = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::GetStreamListener(nsIStreamListener** aListener)
{
  NS_IF_ADDREF(*aListener = mStreamParser);
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::ContinueInterruptedParsing()
{
  // If there are scripts executing, then the content sink is jumping the gun
  // (probably due to a synchronous XMLHttpRequest) and will re-enable us
  // later, see bug 460706.
  if (mExecutor->IsScriptExecuting()) {
    return NS_OK;
  }
  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);
  CancelParsingEvents(); // If the executor caused us to continue, ignore event
  ParseUntilBlocked();
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::BlockParser()
{
  mBlocked = PR_TRUE;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::UnblockParser()
{
  mBlocked = PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsParserEnabled()
{
  return !mBlocked;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsComplete()
{
  return mExecutor->IsComplete();
}

NS_IMETHODIMP
nsHtml5Parser::Parse(nsIURI* aURL, // legacy parameter; ignored
                     nsIRequestObserver* aObserver,
                     void* aKey,
                     nsDTDMode aMode) // legacy; ignored
{
  /*
   * Do NOT cause WillBuildModel to be called synchronously from here!
   * The document won't be ready for it until OnStartRequest!
   */
  NS_PRECONDITION(!mExecutor->HasStarted(), 
                  "Tried to start parse without initializing the parser properly.");
  NS_PRECONDITION(mStreamParser, 
                  "Can't call this variant of Parse() on script-created parser");
  mStreamParser->SetObserver(aObserver);
  mExecutor->SetStreamParser(mStreamParser);
  mExecutor->SetParser(this);
  mRootContextKey = aKey;
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType, // ignored
                     PRBool aLastCall,
                     nsDTDMode aMode) // ignored
{
  NS_PRECONDITION(!mFragmentMode, "Document.write called in fragment mode!");

  // Maintain a reference to ourselves so we don't go away
  // till we're completely done. The old parser grips itself in this method.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  
  // Gripping the other objects just in case, since the other old grip
  // required grips to these, too.
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);

  // Return early if the parser has processed EOF
  if (!mExecutor->HasStarted()) {
    NS_ASSERTION(!mStreamParser,
                 "Had stream parser but document.write started life cycle.");
    mExecutor->SetParser(this);
    mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());
    mTokenizer->start();
    mExecutor->Start();
    /*
     * If you move the following line, be very careful not to cause 
     * WillBuildModel to be called before the document has had its 
     * script global object set.
     */
    mExecutor->WillBuildModel(eDTDMode_unknown);
  }
  if (mExecutor->IsComplete()) {
    return NS_OK;
  }
  if (aLastCall && aSourceBuffer.IsEmpty() && aKey == GetRootContextKey()) {
    // document.close()
    NS_ASSERTION(!mStreamParser,
                 "Had stream parser but got document.close().");
    mDocumentClosed = PR_TRUE;
    if (!mBlocked) {
      ParseUntilBlocked();
    }
    return NS_OK;
  }

  NS_PRECONDITION(IsInsertionPointDefined(), 
                  "Document.write called when insertion point not defined.");

  if (aSourceBuffer.IsEmpty()) {
    return NS_OK;
  }

  PRInt32 lineNumberSave = mTokenizer->getLineNumber();

  nsRefPtr<nsHtml5UTF16Buffer> buffer = new nsHtml5UTF16Buffer(aSourceBuffer.Length());
  memcpy(buffer->getBuffer(), aSourceBuffer.BeginReading(), aSourceBuffer.Length() * sizeof(PRUnichar));
  buffer->setEnd(aSourceBuffer.Length());

  // The buffer is inserted to the stream here in case it won't be parsed
  // to completion.
  // The script is identified by aKey. If there's nothing in the buffer
  // chain for that key, we'll insert at the head of the queue.
  // When the script leaves something in the queue, a zero-length
  // key-holder "buffer" is inserted in the queue. If the same script
  // leaves something in the chain again, it will be inserted immediately
  // before the old key holder belonging to the same script.
  nsHtml5UTF16Buffer* prevSearchBuf = nsnull;
  nsHtml5UTF16Buffer* searchBuf = mFirstBuffer;
  if (aKey) { // after document.open, the first level of document.write has null key
    while (searchBuf != mLastBuffer) {
      if (searchBuf->key == aKey) {
        // found a key holder
        // now insert the new buffer between the previous buffer
        // and the key holder.
        buffer->next = searchBuf;
        if (prevSearchBuf) {
          prevSearchBuf->next = buffer;
        } else {
          mFirstBuffer = buffer;
        }
        break;
      }
      prevSearchBuf = searchBuf;
      searchBuf = searchBuf->next;
    }
  }
  if (searchBuf == mLastBuffer || !aKey) {
    // key was not found or we have a first-level write after document.open
    // we'll insert to the head of the queue
    nsHtml5UTF16Buffer* keyHolder = new nsHtml5UTF16Buffer(aKey);
    keyHolder->next = mFirstBuffer;
    buffer->next = keyHolder;
    mFirstBuffer = buffer;
  }

  if (!mBlocked) {
    // mExecutor->WillResume();
    while (buffer->hasMore()) {
      buffer->adjust(mLastWasCR);
      mLastWasCR = PR_FALSE;
      if (buffer->hasMore()) {
        mLastWasCR = mTokenizer->tokenizeBuffer(buffer);
        if (mTreeBuilder->HasScript()) {
          // No need to flush characters, because an end tag was tokenized last
          mTreeBuilder->Flush(); // Move ops to the executor
          mExecutor->Flush(); // run the ops    
        }
        if (mBlocked) {
          // mExecutor->WillInterrupt();
          break;
        }
        // Ignore suspension requests
      }
    }
  }

  if (!mBlocked) { // buffer was tokenized to completion
    // Scripting semantics require a forced tree builder flush here
    mTreeBuilder->flushCharacters(); // Flush trailing characters
    mTreeBuilder->Flush(); // Move ops to the executor
    mExecutor->Flush(); // run the ops    
  } else if (!mStreamParser && buffer->hasMore() && aKey == mRootContextKey) {
    // The buffer wasn't parsed completely, the document was created by
    // document.open() and the script that wrote wasn't created by this parser. 
    // Can't rely on the executor causing the parser to continue.
    MaybePostContinueEvent();
  }

  mTokenizer->setLineNumber(lineNumberSave);
  return NS_OK;
}

/**
 * This magic value is passed to the previous method on document.close()
 */
NS_IMETHODIMP_(void *)
nsHtml5Parser::GetRootContextKey()
{
  return mRootContextKey;
}

NS_IMETHODIMP
nsHtml5Parser::Terminate(void)
{
  // We should only call DidBuildModel once, so don't do anything if this is
  // the second time that Terminate has been called.
  if (mExecutor->IsComplete()) {
    return NS_OK;
  }
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);
  if (mStreamParser) {
    mStreamParser->Terminate();
  }
  return mExecutor->DidBuildModel(PR_TRUE);
}

NS_IMETHODIMP
nsHtml5Parser::ParseFragment(const nsAString& aSourceBuffer,
                             void* aKey,
                             nsTArray<nsString>& aTagStack,
                             PRBool aXMLMode,
                             const nsACString& aContentType,
                             nsDTDMode aMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::ParseFragment(const nsAString& aSourceBuffer,
                             nsISupports* aTargetNode,
                             nsIAtom* aContextLocalName,
                             PRInt32 aContextNamespace,
                             PRBool aQuirks)
{
  nsCOMPtr<nsIContent> target = do_QueryInterface(aTargetNode);
  NS_ASSERTION(target, "Target did not QI to nsIContent");

  nsIDocument* doc = target->GetOwnerDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_AVAILABLE);
  
  nsIURI* uri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsISupports> container = doc->GetContainer();
  NS_ENSURE_TRUE(container, NS_ERROR_NOT_AVAILABLE);

  Initialize(doc, uri, container, nsnull);

  // Initialize() doesn't deal with base URI
  mExecutor->SetBaseUriFromDocument();
  mExecutor->SetParser(this);
  mExecutor->SetNodeInfoManager(target->GetOwnerDoc()->NodeInfoManager());

  nsIContent* weakTarget = target;
  mTreeBuilder->setFragmentContext(aContextLocalName, aContextNamespace, &weakTarget, aQuirks);
  mFragmentMode = PR_TRUE;
  
  NS_PRECONDITION(!mExecutor->HasStarted(), "Tried to start parse without initializing the parser properly.");
  mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());
  mTokenizer->start();
  mExecutor->Start(); // Don't call WillBuildModel in fragment case
  if (!aSourceBuffer.IsEmpty()) {
    PRBool lastWasCR = PR_FALSE;
    nsHtml5UTF16Buffer buffer(aSourceBuffer.Length());
    memcpy(buffer.getBuffer(), aSourceBuffer.BeginReading(), aSourceBuffer.Length() * sizeof(PRUnichar));
    buffer.setEnd(aSourceBuffer.Length());
    while (buffer.hasMore()) {
      buffer.adjust(lastWasCR);
      lastWasCR = PR_FALSE;
      if (buffer.hasMore()) {
        lastWasCR = mTokenizer->tokenizeBuffer(&buffer);
        mExecutor->MaybePreventExecution();
      }
    }
  }
  mTokenizer->eof();
  mTreeBuilder->StreamEnded();
  mTreeBuilder->Flush();
  mExecutor->Flush();
  mTokenizer->end();
  mExecutor->DropParserAndPerfHint();
  mAtomTable.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::BuildModel(void)
{
  NS_NOTREACHED("Don't call this!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::CancelParsingEvents()
{
  mContinueEvent = nsnull;
  return NS_OK;
}

void
nsHtml5Parser::Reset()
{
  mExecutor->Reset();
  mLastWasCR = PR_FALSE;
  mFragmentMode = PR_FALSE;
  UnblockParser();
  mDocumentClosed = PR_FALSE;
  mStreamParser = nsnull;
  mParserInsertedScriptsBeingEvaluated = 0;
  mRootContextKey = nsnull;
  mContinueEvent = nsnull;  // weak ref
  mAtomTable.Clear(); // should be already cleared in the fragment case anyway
  // Portable parser objects
  mFirstBuffer->next = nsnull;
  mFirstBuffer->setStart(0);
  mFirstBuffer->setEnd(0);
}

PRBool
nsHtml5Parser::CanInterrupt()
{
  return !mFragmentMode;
}

PRBool
nsHtml5Parser::IsInsertionPointDefined()
{
  return !mExecutor->IsFlushing() &&
    (!mStreamParser || mParserInsertedScriptsBeingEvaluated);
}

void
nsHtml5Parser::BeginEvaluatingParserInsertedScript()
{
  ++mParserInsertedScriptsBeingEvaluated;
}

void
nsHtml5Parser::EndEvaluatingParserInsertedScript()
{
  --mParserInsertedScriptsBeingEvaluated;
}

void
nsHtml5Parser::MarkAsNotScriptCreated()
{
  NS_PRECONDITION(!mStreamParser, "Must not call this twice.");
  mStreamParser = new nsHtml5StreamParser(mExecutor, this);
}

PRBool
nsHtml5Parser::IsScriptCreated()
{
  return !mStreamParser;
}

/* End nsIParser  */

// not from interface
void
nsHtml5Parser::HandleParserContinueEvent(nsHtml5ParserContinueEvent* ev)
{
  // Ignore any revoked continue events...
  if (mContinueEvent != ev)
    return;
  mContinueEvent = nsnull;
  NS_ASSERTION(!mExecutor->IsScriptExecuting(), "Interrupted in the middle of a script?");
  ContinueInterruptedParsing();
}

void
nsHtml5Parser::ParseUntilBlocked()
{
  NS_PRECONDITION(!mFragmentMode, "ParseUntilBlocked called in fragment mode.");

  if (mBlocked) {
    return;
  }

  if (mExecutor->IsComplete()) {
    return;
  }
  NS_ASSERTION(mExecutor->HasStarted(), "Bad life cycle.");

  mExecutor->WillResume();
  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        if (mExecutor->IsComplete()) {
          // something like cache manisfests stopped the parse in mid-flight
          return;
        }
        if (mDocumentClosed) {
          NS_ASSERTION(!mStreamParser,
                       "This should only happen with script-created parser.");
          mTokenizer->eof();
          mTreeBuilder->StreamEnded();
          mTreeBuilder->Flush();
          mExecutor->Flush();
          mTokenizer->end();
          return;            
        } else {
          // never release the last buffer.
          NS_ASSERTION(!mLastBuffer->getStart(), 
            "Sentinel buffer had its indeces changed.");
          NS_ASSERTION(!mLastBuffer->getEnd(), 
            "Sentinel buffer had its indeces changed.");
          if (mStreamParser && 
              mReturnToStreamParserPermitted && 
              !mExecutor->IsScriptExecuting()) {
            mReturnToStreamParserPermitted = PR_FALSE;
            mStreamParser->ContinueAfterScripts(mTokenizer, 
                                                mTreeBuilder, 
                                                mLastWasCR);
          }
          return; // no more data for now but expecting more
        }
      } else {
        mFirstBuffer = mFirstBuffer->next;
        continue;
      }
    }

    if (mBlocked || mExecutor->IsComplete()) {
      return;
    }

    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = PR_FALSE;
    if (mFirstBuffer->hasMore()) {
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      if (mTreeBuilder->HasScript()) {
        mTreeBuilder->Flush();
        mExecutor->Flush();   
      }
      if (mBlocked) {
        // mExecutor->WillInterrupt();
        return;
      }
    }
    continue;
  }
}

void
nsHtml5Parser::MaybePostContinueEvent()
{
  NS_PRECONDITION(!mExecutor->IsComplete(), 
                  "Tried to post continue event when the parser is done.");
  if (mContinueEvent) {
    return; // we already have a pending event
  }
  // This creates a reference cycle between this and the event that is
  // broken when the event fires.
  nsCOMPtr<nsIRunnable> event = new nsHtml5ParserContinueEvent(this);
  if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
    NS_WARNING("failed to dispatch parser continuation event");
  } else {
    mContinueEvent = event;
  }
}

nsresult
nsHtml5Parser::Initialize(nsIDocument* aDoc,
                          nsIURI* aURI,
                          nsISupports* aContainer,
                          nsIChannel* aChannel)
{
  if (mStreamParser && aDoc) {
    mStreamParser->SetSpeculativeLoaderWithDocument(aDoc);
  }
  return mExecutor->Init(aDoc, aURI, aContainer, aChannel);
}

void
nsHtml5Parser::StartTokenizer(PRBool aScriptingEnabled) {
  mTreeBuilder->setScriptingEnabled(aScriptingEnabled);
  mTokenizer->start();
}

void
nsHtml5Parser::InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState)
{
  mTokenizer->resetToDataState();
  mTreeBuilder->loadState(aState, &mAtomTable);
  mLastWasCR = PR_FALSE;
  mReturnToStreamParserPermitted = PR_TRUE;
}

void
nsHtml5Parser::ContinueAfterFailedCharsetSwitch()
{
  NS_PRECONDITION(mStreamParser, 
    "Tried to continue after failed charset switch without a stream parser");
  mStreamParser->ContinueAfterFailedCharsetSwitch();
}
