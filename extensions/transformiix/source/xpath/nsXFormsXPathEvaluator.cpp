/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Aaron Reed <aaronr@us.ibm.com>
 *  Merle Sterling <msterlin@us.ibm.com>
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

#include "nsXFormsXPathEvaluator.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIDOMClassInfo.h"
#include "nsXPathException.h"
#include "nsXPathExpression.h"
#include "nsXPathNSResolver.h"
#include "nsXPathResult.h"
#include "nsContentCID.h"
#include "Expr.h"
#include "ExprParser.h"
#include "nsDOMError.h"
#include "txURIUtils.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMString.h"
#include "nsINameSpaceManager.h"
#include "txError.h"
#include "txAtoms.h"
#include "XFormsFunctions.h"
#include "nsIDOM3Node.h"

extern nsINameSpaceManager* gTxNameSpaceManager;

NS_IMPL_ADDREF(nsXFormsXPathEvaluator)
NS_IMPL_RELEASE(nsXFormsXPathEvaluator)
NS_INTERFACE_MAP_BEGIN(nsXFormsXPathEvaluator)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsXPathEvaluator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXFormsXPathEvaluator)
NS_INTERFACE_MAP_END

nsXFormsXPathEvaluator::nsXFormsXPathEvaluator()
{
}

nsXFormsXPathEvaluator::~nsXFormsXPathEvaluator()
{
}

NS_IMETHODIMP
nsXFormsXPathEvaluator::CreateExpression(const nsAString & aExpression, 
                                         nsIDOMNode *aResolverNode,
                                         nsIDOMNode *aOrigCtxt,
                                         nsIDOMNSXPathExpression **aResult)
{
  nsresult rv = NS_OK;
  if (!mRecycler) {
    nsRefPtr<txResultRecycler> recycler = new txResultRecycler;
    NS_ENSURE_TRUE(recycler, NS_ERROR_OUT_OF_MEMORY);
    
    rv = recycler->init();
    NS_ENSURE_SUCCESS(rv, rv);
    
    mRecycler = recycler;
  }

  XFormsParseContextImpl pContext(aResolverNode, aOrigCtxt);
                                  
  nsAutoPtr<Expr> expression;
  rv = txExprParser::createExpr(PromiseFlatString(aExpression), &pContext,
                                getter_Transfers(expression));
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_NAMESPACE_ERR) {
      return NS_ERROR_DOM_NAMESPACE_ERR;
    }

    return NS_ERROR_DOM_INVALID_EXPRESSION_ERR;
  }

  *aResult = new nsXPathExpression(expression, mRecycler);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathEvaluator::Evaluate(const nsAString & aExpression,
                                 nsIDOMNode *aContextNode,
                                 PRUint32 aPosition,
                                 PRUint32 aSize,
                                 nsIDOMNode *aResolverNode,
                                 nsIDOMNode *aOrigCtxt,
                                 PRUint16 aType,
                                 nsISupports *aInResult,
                                 nsISupports **aResult)
{
  // XXX Need to check document of aContextNode if created by
  //   QI'ing a document.

  nsCOMPtr<nsIDOMNSXPathExpression> expression;
  nsresult rv = CreateExpression(aExpression, aResolverNode, aOrigCtxt,
                                 getter_AddRefs(expression));
  NS_ENSURE_SUCCESS(rv, rv);

  return expression->EvaluateWithContext(aContextNode, aPosition, aSize,
                                         aType, aInResult, aResult);
}


/*
 * Implementation of txIParseContext private to nsXFormsXPathEvaluator
 * XFormsParseContextImpl bases on a nsIDOMXPathNSResolver
 */

nsresult nsXFormsXPathEvaluator::XFormsParseContextImpl::resolveNamespacePrefix
    (nsIAtom* aPrefix, PRInt32& aID)
{
  aID = kNameSpaceID_Unknown;

  if (!mResolverNode) {
    return NS_ERROR_DOM_NAMESPACE_ERR;
  }

  nsAutoString prefix;
  if (aPrefix) {
    aPrefix->ToString(prefix);
  }

  nsVoidableString ns;
  nsresult rv;
  // begin - taken directly from nsXPathNSResolver::LookupNamespaceURI
  if (prefix.EqualsLiteral("xml")) {
      ns.AssignLiteral("http://www.w3.org/XML/1998/namespace");
      rv = NS_OK;
  }
  else {
    nsCOMPtr<nsIDOM3Node> dom3Node = do_QueryInterface(mResolverNode);
    NS_ASSERTION(dom3Node, "Need a node to resolve namespaces.");
    if( dom3Node ) {
      rv = dom3Node->LookupNamespaceURI(prefix, ns);
    }
    else {
      SetDOMStringToNull(ns);
      rv = NS_OK;
    }
  }
  // end - taken directly from nsXPathNSResolver::LookupNamespaceURI
  NS_ENSURE_SUCCESS(rv, rv);

  if (DOMStringIsNull(ns)) {
    return NS_ERROR_DOM_NAMESPACE_ERR;
  }

  if (ns.IsEmpty()) {
    aID = kNameSpaceID_None;
    return NS_OK;
  }

  // get the namespaceID for the URI
  return gTxNameSpaceManager->RegisterNameSpace(ns, aID);
}

nsresult
nsXFormsXPathEvaluator::XFormsParseContextImpl::resolveFunctionCall(
                                                    nsIAtom* aName,
                                                    PRInt32 aNamespaceID,
                                                    FunctionCall*& aFnCall)
{
  if (aNamespaceID == kNameSpaceID_None) {
    PRBool isOutOfMem = PR_TRUE;

    if (aName == txXPathAtoms::avg) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::AVG);
    }
    else if (aName == txXPathAtoms::booleanFromString) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::BOOLEANFROMSTRING);
    }
    else if (aName == txXPathAtoms::countNonEmpty) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::COUNTNONEMPTY);
    }
    else if (aName == txXPathAtoms::daysFromDate) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::DAYSFROMDATE);
    }
    else if (aName == txXPathAtoms::ifFunc) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::IF);
    }
    else if (aName == txXPathAtoms::index) {
      NS_ENSURE_TRUE(mResolverNode, NS_ERROR_FAILURE);
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::INDEX,
                                       mResolverNode); 
    }
    else if (aName == txXPathAtoms::instance) {
      NS_ENSURE_TRUE(mResolverNode, NS_ERROR_FAILURE);
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::INSTANCE, 
                                       mResolverNode);
    }
    else if (aName == txXPathAtoms::max) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::MAX);
    }
    else if (aName == txXPathAtoms::min) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::MIN);
    }
    else if (aName == txXPathAtoms::months) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::MONTHS);
    }
    else if (aName == txXPathAtoms::now) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::NOW);
    }
    else if (aName == txXPathAtoms::localDateTime) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::LOCALDATETIME);
    }
    else if (aName == txXPathAtoms::localDate) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::LOCALDATE);
    }
    else if (aName == txXPathAtoms::property) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::PROPERTY);
    }
    else if (aName == txXPathAtoms::seconds) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::SECONDS);
    }
    else if (aName == txXPathAtoms::secondsFromDateTime) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::SECONDSFROMDATETIME);
    }
    else if (aName == txXPathAtoms::current) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::CURRENT,
                                       mOriginalContext);
    }
    else if (aName == txXPathAtoms::event) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::EVENT,
                                       mResolverNode);
    }
    else if (aName == txXPathAtoms::power) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::POWER);
    }
    else if (aName == txXPathAtoms::random) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::RANDOM);
    }
    else if (aName == txXPathAtoms::compare) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::COMPARE);
    }
    else if (aName == txXPathAtoms::context) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::CONTEXT,
                                       mResolverNode);
    }
    else if (aName == txXPathAtoms::daysToDate) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::DAYSTODATE);
    }
    else if (aName == txXPathAtoms::secondsToDateTime) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::SECONDSTODATETIME);
    }
    else if (aName == txXPathAtoms::isCardNumber) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::ISCARDNUMBER);
    }
    else if (aName == txXPathAtoms::digest) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::DIGEST,
                                       mResolverNode);
    }
    else if (aName == txXPathAtoms::adjustDateTimeToTimezone) {
      aFnCall = new XFormsFunctionCall(XFormsFunctionCall::ADJUSTDATETIMETOTIMEZONE);
    }
    else {
      // didn't find functioncall here, aFnCall should be null
      isOutOfMem = PR_FALSE;
    }

    if (aFnCall)
    {
      return NS_OK;
    }
    else if (isOutOfMem) {
      NS_ERROR("XPath FunctionLib failed on out-of-memory");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

PRBool nsXFormsXPathEvaluator::XFormsParseContextImpl::caseInsensitiveNameTests()
{
  // This will always be false since this handles XForms, which is XML-based,
  //   so case sensitive.
  return PR_FALSE;
}

void
nsXFormsXPathEvaluator::XFormsParseContextImpl::SetErrorOffset(PRUint32 aOffset)
{
}