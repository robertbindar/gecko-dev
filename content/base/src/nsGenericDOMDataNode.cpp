/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsRange.h"
#include "nsTextContentChangeData.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsReadableUtils.h"
#include "nsMutationEvent.h"
#include "nsINameSpaceManager.h"
#include "nsIDOM3Node.h"
#include "nsIURI.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMText.h"
#include "nsCOMPtr.h"

#include "pldhash.h"
#include "prprf.h"

// static
void
nsGenericDOMDataNode::Shutdown()
{
}


//----------------------------------------------------------------------

nsGenericDOMDataNode::nsGenericDOMDataNode()
  : mText(), mDocument(nsnull), mParentPtrBits(0)
{
}

nsGenericDOMDataNode::~nsGenericDOMDataNode()
{
  if (HasEventListenerManager()) {
    PL_DHashTableOperate(&nsGenericElement::sEventListenerManagersHash,
                         this, PL_DHASH_REMOVE);
  }

  if (HasRangeList()) {
    PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                         this, PL_DHASH_REMOVE);
  }
}


NS_IMPL_ADDREF(nsGenericDOMDataNode)
NS_IMPL_RELEASE(nsGenericDOMDataNode)

NS_INTERFACE_MAP_BEGIN(nsGenericDOMDataNode)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver)) ||
      aIID.Equals(NS_GET_IID(nsIDOMEventTarget))) {
    foundInterface = NS_STATIC_CAST(nsIDOMEventReceiver *,
                                    nsDOMEventRTTearoff::Create(this));
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);
  } else
  if (aIID.Equals(NS_GET_IID(nsIDOM3EventTarget))) {
    foundInterface = NS_STATIC_CAST(nsIDOM3EventTarget *,
                                    nsDOMEventRTTearoff::Create(this));
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);
  } else
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  // No nsITextContent since all subclasses might not want that.
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, nsNode3Tearoff(this))
NS_INTERFACE_MAP_END


nsresult
nsGenericDOMDataNode::GetNodeValue(nsAString& aNodeValue)
{
  return GetData(aNodeValue);
}

nsresult
nsGenericDOMDataNode::SetNodeValue(const nsAString& aNodeValue)
{
  return SetData(aNodeValue);
}

nsresult
nsGenericDOMDataNode::GetParentNode(nsIDOMNode** aParentNode)
{
  nsresult res = NS_OK;

  nsIContent *parent_weak = GetParentWeak();

  if (parent_weak) {
    res = CallQueryInterface(parent_weak, aParentNode);
  } else if (mDocument) {
    // If we don't have a parent, but we're in the document, we must
    // be the root node of the document. The DOM says that the root
    // is the document.
    res = CallQueryInterface(mDocument, aParentNode);
  } else {
    *aParentNode = nsnull;
  }

  NS_ASSERTION(NS_OK == res, "Must be a DOM Node");

  return res;
}

nsresult
nsGenericDOMDataNode::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  nsresult rv = NS_OK;

  nsIContent *parent_weak = GetParentWeak();
  nsIContent *sibling = nsnull;

  if (parent_weak) {
    PRInt32 pos = parent_weak->IndexOf(this);
    if (pos > 0) {
      sibling = parent_weak->GetChildAt(pos - 1);
    }
  } else if (mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos = mDocument->IndexOf(this);
    if (pos > 0) {
      sibling = mDocument->GetChildAt(pos - 1);
    }
  }

  if (sibling) {
    rv = CallQueryInterface(sibling, aPrevSibling);
    NS_ASSERTION(rv == NS_OK, "Must be a DOM Node");
  } else {
    *aPrevSibling = nsnull;
  }

  return rv;
}

nsresult
nsGenericDOMDataNode::GetNextSibling(nsIDOMNode** aNextSibling)
{
  nsresult rv = NS_OK;

  nsIContent *parent_weak = GetParentWeak();
  nsIContent *sibling = nsnull;

  if (parent_weak) {
    PRInt32 pos = parent_weak->IndexOf(this);
    if (pos > -1 ) {
      sibling = parent_weak->GetChildAt(pos + 1);
    }
  }
  else if (mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos = mDocument->IndexOf(this);
    if (pos > -1 ) {
      sibling = mDocument->GetChildAt(pos + 1);
    }
  }

  if (sibling) {
    rv = CallQueryInterface(sibling, aNextSibling);
    NS_ASSERTION(rv == NS_OK, "Must be a DOM Node");
  } else {
    *aNextSibling = nsnull;
  }

  return rv;
}

nsresult
nsGenericDOMDataNode::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  // XXX Since we believe this won't be done very often, we won't
  // burn another slot in the data node and just create a new
  // (empty) childNodes list every time we're asked.
  nsChildContentList* list = new nsChildContentList(nsnull);
  if (!list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(list, aChildNodes);
}

nsresult
nsGenericDOMDataNode::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  // XXX Actually the owner document is the document in whose context
  // the node has been created. We should be able to get at it
  // whether or not we are attached to the document.
  if (mDocument) {
    return CallQueryInterface(mDocument, aOwnerDocument);
  }

  *aOwnerDocument = nsnull;
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetNamespaceURI(nsAString& aNamespaceURI)
{
  SetDOMStringToNull(aNamespaceURI);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetPrefix(nsAString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetPrefix(const nsAString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

nsresult
nsGenericDOMDataNode::GetLocalName(nsAString& aLocalName)
{
  SetDOMStringToNull(aLocalName);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::Normalize()
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::IsSupported(const nsAString& aFeature,
                                  const nsAString& aVersion,
                                  PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

nsresult
nsGenericDOMDataNode::GetBaseURI(nsAString& aURI)
{
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = GetBaseURL(getter_AddRefs(baseURI));

  nsCAutoString spec;
  if (NS_SUCCEEDED(rv) && baseURI) {
    baseURI->GetSpec(spec);
  }
  CopyUTF8toUTF16(spec, aURI);

  return rv;
}

nsresult
nsGenericDOMDataNode::LookupPrefix(const nsAString& aNamespaceURI,
                                   nsAString& aPrefix)
{
  aPrefix.Truncate();

  nsIContent *parent_weak = GetParentWeak();

  // DOM Data Node passes the query on to its parent
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(parent_weak));
  if (node) {
    return node->LookupPrefix(aNamespaceURI, aPrefix);
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                         nsAString& aNamespaceURI)
{
  aNamespaceURI.Truncate();

  nsIContent *parent_weak = GetParentWeak();

  // DOM Data Node passes the query on to its parent
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(parent_weak));

  if (node) {
    return node->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// Implementation of nsIDOMCharacterData

nsresult
nsGenericDOMDataNode::GetData(nsAString& aData)
{
  if (mText.Is2b()) {
    aData.Assign(mText.Get2b(), mText.GetLength());
  } else {
    // Must use Substring() since nsDependentCString() requires null
    // terminated strings.

    const char *data = mText.Get1b();

    if (data) {
      CopyASCIItoUCS2(Substring(data, data + mText.GetLength()), aData);
    } else {
      aData.Truncate();
    }
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetData(const nsAString& aData)
{
  // inform any enclosed ranges of change
  // we can lie and say we are deleting all the text, since in a total
  // text replacement we should just collapse all the ranges.

  if (HasRangeList()) {
    nsRange::TextOwnerChanged(this, 0, mText.GetLength(), 0);
  }

  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(this);

  return SetText(aData, PR_TRUE);
}

nsresult
nsGenericDOMDataNode::GetLength(PRUint32* aLength)
{
  *aLength = mText.GetLength();
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SubstringData(PRUint32 aStart, PRUint32 aCount,
                                    nsAString& aReturn)
{
  aReturn.Truncate();

  // XXX add <0 checks if types change
  PRUint32 textLength = PRUint32( mText.GetLength() );
  if (aStart > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  PRUint32 amount = aCount;
  if (amount > textLength - aStart) {
    amount = textLength - aStart;
  }

  if (mText.Is2b()) {
    aReturn.Assign(mText.Get2b() + aStart, amount);
  } else {
    // Must use Substring() since nsDependentCString() requires null
    // terminated strings.

    const char *data = mText.Get1b() + aStart;
    CopyASCIItoUCS2(Substring(data, data + amount), aReturn);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsGenericDOMDataNode::AppendData(const nsAString& aData)
{
#if 1
  nsresult rv = NS_OK;
  PRInt32 length = 0;

  // See bugzilla bug 77585.
  if (mText.Is2b() || (!IsASCII(aData))) {
    nsAutoString old_data;
    mText.AppendTo(old_data);
    length = old_data.Length();
    // XXXjag We'd like to just say |old_data + aData|, but due
    // to issues with dependent concatenation and sliding (sub)strings
    // we'll just have to copy for now. See bug 121841 for details.
    old_data.Append(aData);
    rv = SetText(old_data, PR_FALSE);
  } else {
    // We know aData and the current data is ASCII, so use a
    // nsC*String, no need for any fancy unicode stuff here.
    nsCAutoString old_data;
    mText.AppendTo(old_data);
    length = old_data.Length();
    old_data.AppendWithConversion(aData);
    rv = SetText(old_data.get(), old_data.Length(), PR_FALSE);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // Trigger a reflow
  if (mDocument) {
    // This seems bad, why do we need to *allocate* a
    // nsTextContentChangeData thingy here? Could it not be on the stack?

    nsTextContentChangeData* tccd = nsnull;
    rv = NS_NewTextContentChangeData(&tccd);
    if (NS_SUCCEEDED(rv)) {
      tccd->SetData(nsITextContentChangeData::Append, length,
                    aData.Length());
      rv = mDocument->ContentChanged(this, tccd);
      NS_RELEASE(tccd);
    } else {
      rv = mDocument->ContentChanged(this, nsnull);
    }
  }

  return rv;
#else
  return ReplaceData(mText.GetLength(), 0, aData);
#endif
}

nsresult
nsGenericDOMDataNode::InsertData(PRUint32 aOffset,
                                 const nsAString& aData)
{
  return ReplaceData(aOffset, 0, aData);
}

nsresult
nsGenericDOMDataNode::DeleteData(PRUint32 aOffset, PRUint32 aCount)
{
  nsAutoString empty;
  return ReplaceData(aOffset, aCount, empty);
}

nsresult
nsGenericDOMDataNode::ReplaceData(PRUint32 aOffset, PRUint32 aCount,
                                  const nsAString& aData)
{
  nsresult result = NS_OK;

  // sanitize arguments
  PRUint32 textLength = mText.GetLength();
  if (aOffset > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Allocate new buffer
  PRUint32 endOffset = aOffset + aCount;
  if (endOffset > textLength) {
    aCount = textLength - aOffset;
    endOffset = textLength;
  }
  PRInt32 dataLength = aData.Length();
  PRInt32 newLength = textLength - aCount + dataLength;
  PRUnichar* to = new PRUnichar[newLength + 1];
  if (!to) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // inform any enclosed ranges of change

  if (HasRangeList()) {
    nsRange::TextOwnerChanged(this, aOffset, endOffset, dataLength);
  }

  // Copy over appropriate data
  if (0 != aOffset) {
    mText.CopyTo(to, 0, aOffset);
  }
  if (0 != dataLength) {
    CopyUnicodeTo(aData, 0, to+aOffset, dataLength);
  }
  if (endOffset != textLength) {
    mText.CopyTo(to + aOffset + dataLength, endOffset, textLength - endOffset);
  }

  // Null terminate the new buffer...
  to[newLength] = (PRUnichar)0;

  result = SetText(to, newLength, PR_TRUE);
  delete [] to;

  return result;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsGenericDOMDataNode::GetListenerManager(nsIEventListenerManager** aResult)
{
  nsCOMPtr<nsIEventListenerManager> listener_manager;
  LookupListenerManager(getter_AddRefs(listener_manager));

  if (listener_manager) {
    *aResult = listener_manager;
    NS_ADDREF(*aResult);

    return NS_OK;
  }

  if (!nsGenericElement::sEventListenerManagersHash.ops) {
    nsresult rv = nsGenericElement::InitHashes();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv = NS_NewEventListenerManager(aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add a mapping to the hash table
  EventListenerManagerMapEntry *entry =
    NS_STATIC_CAST(EventListenerManagerMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::
                                        sEventListenerManagersHash, this,
                                        PL_DHASH_ADD));

  entry->mListenerManager = *aResult;

  entry->mListenerManager->SetListenerTarget(this);

  SetHasEventListenerManager(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::DoneCreatingElement()
{
  return NS_OK;
}
//----------------------------------------------------------------------

// Implementation of nsIContent

#ifdef DEBUG
void
nsGenericDOMDataNode::ToCString(nsAString& aBuf, PRInt32 aOffset,
                                PRInt32 aLen) const
{
  if (mText.Is2b()) {
    const PRUnichar* cp = mText.Get2b() + aOffset;
    const PRUnichar* end = cp + aLen;

    while (cp < end) {
      PRUnichar ch = *cp++;
      if (ch == '\r') {
        aBuf.Append(NS_LITERAL_STRING("\\r"));
      } else if (ch == '\n') {
        aBuf.Append(NS_LITERAL_STRING("\\n"));
      } else if (ch == '\t') {
        aBuf.Append(NS_LITERAL_STRING("\\t"));
      } else if ((ch < ' ') || (ch >= 127)) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "\\u%04x", ch);
        aBuf.Append(NS_ConvertASCIItoUCS2(buf));
      } else {
        aBuf.Append(ch);
      }
    }
  } else {
    unsigned char* cp = (unsigned char*)mText.Get1b() + aOffset;
    const unsigned char* end = cp + aLen;

    while (cp < end) {
      PRUnichar ch = *cp++;
      if (ch == '\r') {
        aBuf.Append(NS_LITERAL_STRING("\\r"));
      } else if (ch == '\n') {
        aBuf.Append(NS_LITERAL_STRING("\\n"));
      } else if (ch == '\t') {
        aBuf.Append(NS_LITERAL_STRING("\\t"));
      } else if ((ch < ' ') || (ch >= 127)) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "\\u%04x", ch);
        aBuf.Append(NS_ConvertASCIItoUCS2(buf));
      } else {
        aBuf.Append(ch);
      }
    }
  }
}
#endif

NS_IMETHODIMP_(nsIDocument*)
nsGenericDOMDataNode::GetDocument() const
{
  return mDocument;
}


NS_IMETHODIMP
nsGenericDOMDataNode::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                  PRBool aCompileEventHandlers)
{
  mDocument = aDocument;

  if (mDocument && mText.IsBidi()) {
    mDocument->SetBidiEnabled(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP_(nsIContent*)
nsGenericDOMDataNode::GetParent() const
{
  return GetParentWeak();
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetParent(nsIContent* aParent)
{
  PtrBits new_bits = NS_REINTERPRET_CAST(PtrBits, aParent);

  new_bits |= mParentPtrBits & PARENT_BIT_MASK;

  mParentPtrBits = new_bits;

  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsGenericDOMDataNode::IsNativeAnonymous() const
{
  nsIContent* parent = GetParentWeak();
  return parent && parent->IsNativeAnonymous();
}

NS_IMETHODIMP_(void)
nsGenericDOMDataNode::SetNativeAnonymous(PRBool aAnonymous)
{
  // XXX Need to fix this to do something - bug 165110
}

NS_IMETHODIMP
nsGenericDOMDataNode::GetNameSpaceID(PRInt32* aID) const
{
  *aID = kNameSpaceID_None;
  return NS_OK;
}

NS_IMETHODIMP_(nsIAtom*)
nsGenericDOMDataNode::GetIDAttributeName() const
{
  return nsnull;
}

NS_IMETHODIMP_(nsIAtom*)
nsGenericDOMDataNode::GetClassAttributeName() const
{
  return nsnull;
}

NS_IMETHODIMP
nsGenericDOMDataNode::NormalizeAttrString(const nsAString& aStr,
                                          nsINodeInfo** aNodeInfo)
{
  *aNodeInfo = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr,
                              const nsAString& aValue, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetAttr(nsINodeInfo *aNodeInfo,
                              const nsAString& aValue, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr,
                                PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttr,
                              nsAString& aResult) const
{
  aResult.Truncate();

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsGenericDOMDataNode::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttr,
                              nsIAtom** aPrefix,
                              nsAString& aResult) const
{
  *aPrefix = nsnull;
  aResult.Truncate();

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP_(PRBool)
nsGenericDOMDataNode::HasAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute) const
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsGenericDOMDataNode::GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                    nsIAtom** aName, nsIAtom** aPrefix) const
{
  *aNameSpaceID = kNameSpaceID_None;
  *aName = nsnull;
  *aPrefix = nsnull;

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP_(PRUint32)
nsGenericDOMDataNode::GetAttrCount() const
{
  return 0;
}

NS_IMETHODIMP
nsGenericDOMDataNode::HandleDOMEvent(nsIPresContext* aPresContext,
                                     nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  PRBool externalDOMEvent = PR_FALSE;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (!aDOMEvent) {
      aDOMEvent = &domEvent;
    } else {
      externalDOMEvent = PR_TRUE;
    }

    aEvent->flags |= aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
    aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;
  }

  nsIContent *parent_weak = GetParentWeak();

  //Capturing stage evaluation
  if (NS_EVENT_FLAG_CAPTURE & aFlags) {
    //Initiate capturing phase.  Special case first call to document
    if (parent_weak) {
      parent_weak->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                  aFlags & NS_EVENT_CAPTURE_MASK, aEventStatus);
    } else if (mDocument) {
      ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      aFlags & NS_EVENT_CAPTURE_MASK, aEventStatus);
    }
  }

  nsCOMPtr<nsIEventListenerManager> listener_manager;
  LookupListenerManager(getter_AddRefs(listener_manager));

  //Local handling stage
  //Check for null ELM, check if we're a non-bubbling event in the bubbling state (bubbling state
  //is indicated by the presence of the NS_EVENT_FLAG_BUBBLE flag and not the NS_EVENT_FLAG_INIT), and check 
  //if we're a no content dispatch event
  if (listener_manager &&
       !(NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags && NS_EVENT_FLAG_BUBBLE & aFlags && !(NS_EVENT_FLAG_INIT & aFlags)) &&
       !(aEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH)) {
    aEvent->flags |= aFlags;
    listener_manager->HandleEvent(aPresContext, aEvent, aDOMEvent, nsnull,
                                  aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_BUBBLE & aFlags && parent_weak) {
    ret = parent_weak->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      aFlags & NS_EVENT_BUBBLE_MASK, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.

    if (!externalDOMEvent && *aDOMEvent) {
      if (0 != (*aDOMEvent)->Release()) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.

        nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
          do_QueryInterface(*aDOMEvent);

        if (privateEvent) {
          privateEvent->DuplicatePrivateData();
        }
      }
    }

    aDOMEvent = nsnull;
  }

  return ret;
}

NS_IMETHODIMP
nsGenericDOMDataNode::GetContentID(PRUint32* aID)
{
  *aID = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetContentID(PRUint32 aID)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsINodeInfo *)
nsGenericDOMDataNode::GetNodeInfo() const
{
  return nsnull;
}

NS_IMETHODIMP_(PRBool)
nsGenericDOMDataNode::CanContainChildren() const
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRUint32)
nsGenericDOMDataNode::GetChildCount() const
{
  return 0;
}

NS_IMETHODIMP_(nsIContent *)
nsGenericDOMDataNode::GetChildAt(PRUint32 aIndex) const
{
  return nsnull;
}

NS_IMETHODIMP_(PRInt32)
nsGenericDOMDataNode::IndexOf(nsIContent* aPossibleChild) const
{
  return -1;
}

NS_IMETHODIMP
nsGenericDOMDataNode::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                    PRBool aNotify, PRBool aDeepSetDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::ReplaceChildAt(nsIContent* aKid, PRUint32 aIndex,
                                     PRBool aNotify, PRBool aDeepSetDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                    PRBool aDeepSetDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::RangeAdd(nsIDOMRange* aRange)
{
  // lazy allocation of range list

  if (!nsGenericElement::sRangeListsHash.ops) {
    nsresult rv = nsGenericElement::InitHashes();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                                        this, PL_DHASH_ADD));

  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsVoidArray *range_list = entry->mRangeList;

  if (!range_list) {
    range_list = new nsAutoVoidArray();

    if (!range_list) {
      PL_DHashTableRawRemove(&nsGenericElement::sRangeListsHash, entry);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    entry->mRangeList = range_list;

    SetHasRangeList(PR_TRUE);
  } else {
    // Make sure we don't add a range that is already
    // in the list!
    PRInt32 i = range_list->IndexOf(aRange);

    if (i >= 0) {
      // Range is already in the list, so there is nothing to do!

      return NS_OK;
    }
  }

  // dont need to addref - this call is made by the range object itself
  PRBool rv = range_list->AppendElement(aRange);

  return rv ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsGenericDOMDataNode::RangeRemove(nsIDOMRange* aRange)
{
  RangeListMapEntry *entry = nsnull;

  if (HasRangeList()) {
    entry =
      NS_STATIC_CAST(RangeListMapEntry *,
                     PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                                          this, PL_DHASH_LOOKUP));
  }

  if (entry && PL_DHASH_ENTRY_IS_BUSY(entry)) {
    // dont need to release - this call is made by the range object
    // itself
    PRBool rv = entry->mRangeList->RemoveElement(aRange);

    if (rv) {
      if (entry->mRangeList->Count() == 0) {
        PL_DHashTableRawRemove(&nsGenericElement::sRangeListsHash, entry);

        SetHasRangeList(PR_FALSE);
      }

      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsGenericDOMDataNode::GetRangeList(nsVoidArray** aResult) const
{
  *aResult = LookupRangeList();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::RemoveFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsIContent*)
nsGenericDOMDataNode::GetBindingParent() const
{
  return nsnull;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetBindingParent(nsIContent* aParent)
{
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsGenericDOMDataNode::IsContentOfType(PRUint32 aFlags)
{
  return PR_FALSE;
}

#ifdef DEBUG
NS_IMETHODIMP
nsGenericDOMDataNode::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::DumpContent(FILE* out, PRInt32 aIndent,
                                  PRBool aDumpAll) const 
{
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsGenericDOMDataNode::GetBaseURL(nsIURI** aURI) const
{
  // DOM Data Node inherits the base from its parent element/document
  nsIContent* parent_weak = GetParentWeak();
  if (parent_weak) {
    return parent_weak->GetBaseURL(aURI);
  }

  if (mDocument) {
    return mDocument->GetBaseURL(aURI);
  }

  *aURI = nsnull;
  return NS_OK;
}

//----------------------------------------------------------------------

// Implementation of the nsIDOMText interface

nsresult
nsGenericDOMDataNode::SplitText(PRUint32 aOffset, nsIDOMText** aReturn)
{
  nsresult rv = NS_OK;
  nsAutoString cutText;
  PRUint32 length;

  GetLength(&length);
  if (aOffset > length) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  rv = SubstringData(aOffset, length - aOffset, cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = DeleteData(aOffset, length - aOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /*
   * Use CloneContent() for creating the new node so that the new node is of
   * same class as this node!
   */

  nsCOMPtr<nsITextContent> newContent;
  rv = CloneContent(PR_FALSE, getter_AddRefs(newContent));
  if (NS_FAILED(rv)) {
    return rv;
  }


  nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(newContent, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = newNode->SetNodeValue(cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIContent* parentNode = GetParentWeak();

  if (parentNode) {
    PRInt32 index = parentNode->IndexOf(this);

    nsCOMPtr<nsIContent> content(do_QueryInterface(newNode));

    parentNode->InsertChildAt(content, index+1, PR_TRUE, PR_FALSE);
  }

  return CallQueryInterface(newNode, aReturn);
}

//----------------------------------------------------------------------

// Implementation of the nsITextContent interface

NS_IMETHODIMP
nsGenericDOMDataNode::GetText(const nsTextFragment** aFragmentsResult)
{
  *aFragmentsResult = &mText;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::GetTextLength(PRInt32* aLengthResult)
{
  if (!aLengthResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLengthResult = mText.GetLength();

  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::CopyText(nsAString& aResult)
{
  if (mText.Is2b()) {
    aResult.Assign(mText.Get2b(), mText.GetLength());
  } else {
    // Must use Substring() since nsDependentCString() requires null
    // terminated strings.

    const char *data = mText.Get1b();
    CopyASCIItoUCS2(Substring(data, data + mText.GetLength()), aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetText(const PRUnichar* aBuffer,
                              PRInt32 aLength,
                              PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && aBuffer, "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (!aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);

  mText.SetTo(aBuffer, aLength);

  SetBidiStatus();

  if (mDocument && nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(this));
    nsMutationEvent mutation;
    mutation.eventStructType = NS_MUTATION_EVENT;
    mutation.message = NS_MUTATION_CHARACTERDATAMODIFIED;
    mutation.mTarget = node;

    // XXX Handle the setting of prevValue!
    nsAutoString newVal(aBuffer);
    if (!newVal.IsEmpty())
      mutation.mNewAttrValue = do_GetAtom(newVal);
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && mDocument) {
    mDocument->ContentChanged(this, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetText(const char* aBuffer, PRInt32 aLength,
                              PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && aBuffer, "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (!aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);

  mText.SetTo(aBuffer, aLength);

  if (mDocument && nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(this));
    nsMutationEvent mutation;
    mutation.eventStructType = NS_MUTATION_EVENT;
    mutation.message = NS_MUTATION_CHARACTERDATAMODIFIED;
    mutation.mTarget = node;

    // XXX Handle the setting of prevValue!
    nsAutoString newVal; newVal.AssignWithConversion(aBuffer);
    if (!newVal.IsEmpty())
      mutation.mNewAttrValue = do_GetAtom(newVal);
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && mDocument) {
    mDocument->ContentChanged(this, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetText(const nsAString& aStr,
                              PRBool aNotify)
{
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);

  mText = aStr;

  SetBidiStatus();

  if (mDocument && nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(this));
    nsMutationEvent mutation;
    mutation.eventStructType = NS_MUTATION_EVENT;
    mutation.message = NS_MUTATION_CHARACTERDATAMODIFIED;
    mutation.mTarget = node;

    // XXX Handle the setting of prevValue!
    nsAutoString newVal(aStr);
    if (!newVal.IsEmpty())
      mutation.mNewAttrValue = do_GetAtom(newVal);
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && mDocument) {
    mDocument->ContentChanged(this, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::IsOnlyWhitespace(PRBool* aResult)
{
  nsTextFragment& frag = mText;
  if (frag.Is2b()) {
    const PRUnichar* cp = frag.Get2b();
    const PRUnichar* end = cp + frag.GetLength();

    while (cp < end) {
      PRUnichar ch = *cp++;

      if (!XP_IS_SPACE(ch)) {
        *aResult = PR_FALSE;

        return NS_OK;
      }
    }
  } else {
    const char* cp = frag.Get1b();
    const char* end = cp + frag.GetLength();

    while (cp < end) {
      PRUnichar ch = PRUnichar(*(unsigned char*)cp);
      ++cp;

      if (!XP_IS_SPACE(ch)) {
        *aResult = PR_FALSE;

        return NS_OK;
      }
    }
  }

  *aResult = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericDOMDataNode::CloneContent(PRBool aCloneText, nsITextContent** aClone)
{
  NS_ERROR("Override me!");

  *aClone = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericDOMDataNode::AppendTextTo(nsAString& aResult)
{
  if (mText.Is2b()) {
    aResult.Append(mText.Get2b(), mText.GetLength());
  } else {

    // XXX we would like to have a AppendASCIItoUCS2 here

    aResult.Append(NS_ConvertASCIItoUCS2(mText.Get1b(),
                                         mText.GetLength()).get(),
                   mText.GetLength());
  }

  return NS_OK;
}

void
nsGenericDOMDataNode::LookupListenerManager(nsIEventListenerManager **aListenerManager) const
{
  *aListenerManager = nsnull;

  if (!HasEventListenerManager()) {
    return;
  }

  EventListenerManagerMapEntry *entry =
    NS_STATIC_CAST(EventListenerManagerMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::
                                        sEventListenerManagersHash, this,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    *aListenerManager = entry->mListenerManager;
    NS_ADDREF(*aListenerManager);
  }
}

nsVoidArray *
nsGenericDOMDataNode::LookupRangeList() const
{
  if (!HasRangeList()) {
    return nsnull;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                                        this, PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    return entry->mRangeList;
  }

  return nsnull;
}

void nsGenericDOMDataNode::SetBidiStatus()
{
  if (mDocument) {
    PRBool isBidiDocument = PR_FALSE;

    mDocument->GetBidiEnabled(&isBidiDocument);

    if (isBidiDocument) {
      // OK, we already know it's Bidi, so we won't test again

      return;
    }
  }

  mText.SetBidiFlag();

  if (mDocument && mText.IsBidi()) {
    mDocument->SetBidiEnabled(PR_TRUE);
  }
}
