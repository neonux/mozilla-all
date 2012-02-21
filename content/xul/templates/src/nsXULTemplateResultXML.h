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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsXULTemplateResultXML_h__
#define nsXULTemplateResultXML_h__

#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIRDFResource.h"
#include "nsXULTemplateQueryProcessorXML.h"
#include "nsIXULTemplateResult.h"

/**
 * An single result of an query
 */
class nsXULTemplateResultXML : public nsIXULTemplateResult
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIXULTEMPLATERESULT

    nsXULTemplateResultXML(nsXMLQuery* aQuery,
                           nsIDOMNode* aNode,
                           nsXMLBindingSet* aBindings);

    ~nsXULTemplateResultXML() {}

    void GetNode(nsIDOMNode** aNode);

protected:

    // ID used for persisting data. It is constructed using the mNode's
    // base uri plus the node's id to form 'baseuri#id'. If the node has no
    // id, then an id of the form 'row<some number>' is generated. In the
    // latter case, persistence will not work as there won't be a unique id.
    nsAutoString mId;

    // query that generated the result
    nsCOMPtr<nsXMLQuery> mQuery;

    // context node in datasource
    nsCOMPtr<nsIDOMNode> mNode;

    // assignments in query
    nsXMLBindingValues mRequiredValues;

    // extra assignments made by rules (<binding> tags)
    nsXMLBindingValues mOptionalValues;
};

#endif // nsXULTemplateResultXML_h__