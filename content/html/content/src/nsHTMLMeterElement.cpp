/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
 *   Vincent Lamotte <Vincent.Lamotte@ensimag.imag.fr>
 *   Laurent Dulary <Laurent.Dulary@ensimag.imag.fr>
 *   Yoan Teboul <Yoan.Teboul@ensimag.imag.fr>
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
 
#include "nsIDOMHTMLMeterElement.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsEventStateManager.h"
#include "nsAlgorithm.h"


class nsHTMLMeterElement : public nsGenericHTMLFormElement,
                           public nsIDOMHTMLMeterElement
{
public:
  nsHTMLMeterElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLMeterElement();

  /* nsISupports */
  NS_DECL_ISUPPORTS_INHERITED

  /* nsIDOMNode */
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  /* nsIDOMElement */
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  /* nsIDOMHTMLElement */
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  /* nsIDOMHTMLMeterElement */
  NS_DECL_NSIDOMHTMLMETERELEMENT

  /* nsIFormControl */
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_METER; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  bool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                      const nsAString& aValue, nsAttrValue& aResult);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

private:

  static const double kDefaultValue;
  static const double kDefaultMin;
  static const double kDefaultMax;

  /* @return the minimum value */
  double GetMin() const;

  /* @return the maximum value */
  double GetMax() const;

  /* @return the actual value */
  double GetValue() const;

  /* @return the low value */
  double GetLow() const;

  /* @return the high value */
  double GetHigh() const;

  /* @return the optimum value */
  double GetOptimum() const;
};

const double nsHTMLMeterElement::kDefaultValue =  0.0;
const double nsHTMLMeterElement::kDefaultMin   =  0.0;
const double nsHTMLMeterElement::kDefaultMax   =  1.0;

NS_IMPL_NS_NEW_HTML_ELEMENT(Meter)


nsHTMLMeterElement::nsHTMLMeterElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
{
}

nsHTMLMeterElement::~nsHTMLMeterElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLMeterElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLMeterElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLMeterElement, nsHTMLMeterElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLMeterElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLMeterElement,
                                   nsIDOMHTMLMeterElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLMeterElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLMeterElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLMeterElement)


NS_IMETHODIMP
nsHTMLMeterElement::Reset()
{
  /* The meter element is not resettable. */
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  /* The meter element is not submittable. */
  return NS_OK;
}

bool
nsHTMLMeterElement::ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                 const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max ||
        aAttribute == nsGkAtoms::min   || aAttribute == nsGkAtoms::low ||
        aAttribute == nsGkAtoms::high  || aAttribute == nsGkAtoms::optimum) {
      return aResult.ParseDoubleValue(aValue);
    }
  }

  return nsGenericHTMLFormElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

/*
 * Value getters :
 * const getters used by XPCOM methods and by IntrinsicState
 */

double
nsHTMLMeterElement::GetMin() const
{
  /**
   * If the attribute min is defined, the minimum is this value.
   * Otherwise, the minimum is the default value.
   */
  const nsAttrValue* attrMin = mAttrsAndChildren.GetAttr(nsGkAtoms::min);
  if (attrMin && attrMin->Type() == nsAttrValue::eDoubleValue) {
    return attrMin->GetDoubleValue();
  }
  return kDefaultMin;
}

double
nsHTMLMeterElement::GetMax() const
{
  /**
   * If the attribute max is defined, the maximum is this value.
   * Otherwise, the maximum is the default value.
   * If the maximum value is less than the minimum value,
   * the maximum value is the same as the minimum value.
   */
  double max;

  const nsAttrValue* attrMax = mAttrsAndChildren.GetAttr(nsGkAtoms::max);
  if (attrMax && attrMax->Type() == nsAttrValue::eDoubleValue) {
    max = attrMax->GetDoubleValue();
  } else {
    max = kDefaultMax;
  }

  return NS_MAX(max, GetMin());
}

double
nsHTMLMeterElement::GetValue() const
{
  /**
   * If the attribute value is defined, the actual value is this value.
   * Otherwise, the actual value is the default value.
   * If the actual value is less than the minimum value,
   * the actual value is the same as the minimum value.
   * If the actual value is greater than the maximum value,
   * the actual value is the same as the maximum value.
   */
  double value;

  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  if (attrValue && attrValue->Type() == nsAttrValue::eDoubleValue) {
    value = attrValue->GetDoubleValue();
  } else {
    value = kDefaultValue;
  }

  double min = GetMin();

  if (value <= min) {
    return min;
  }

  return NS_MIN(value, GetMax());
}

double
nsHTMLMeterElement::GetLow() const
{
  /**
   * If the low value is defined, the low value is this value.
   * Otherwise, the low value is the minimum value.
   * If the low value is less than the minimum value,
   * the low value is the same as the minimum value.
   * If the low value is greater than the maximum value,
   * the low value is the same as the maximum value.
   */

  double min = GetMin();

  const nsAttrValue* attrLow = mAttrsAndChildren.GetAttr(nsGkAtoms::low);
  if (!attrLow || attrLow->Type() != nsAttrValue::eDoubleValue) {
    return min;
  }

  double low = attrLow->GetDoubleValue();

  if (low <= min) {
    return min;
  }

  return NS_MIN(low, GetMax());
}

double
nsHTMLMeterElement::GetHigh() const
{
  /**
   * If the high value is defined, the high value is this value.
   * Otherwise, the high value is the maximum value.
   * If the high value is less than the low value,
   * the high value is the same as the low value.
   * If the high value is greater than the maximum value,
   * the high value is the same as the maximum value.
   */

  double max = GetMax();

  const nsAttrValue* attrHigh = mAttrsAndChildren.GetAttr(nsGkAtoms::high);
  if (!attrHigh || attrHigh->Type() != nsAttrValue::eDoubleValue) {
    return max;
  }

  double high = attrHigh->GetDoubleValue();

  if (high >= max) {
    return max;
  }

  return NS_MAX(high, GetLow());
}

double
nsHTMLMeterElement::GetOptimum() const
{
  /**
   * If the optimum value is defined, the optimum value is this value.
   * Otherwise, the optimum value is the midpoint between
   * the minimum value and the maximum value :
   * min + (max - min)/2 = (min + max)/2
   * If the optimum value is less than the minimum value,
   * the optimum value is the same as the minimum value.
   * If the optimum value is greater than the maximum value,
   * the optimum value is the same as the maximum value.
   */

  double max = GetMax();

  double min = GetMin();

  const nsAttrValue* attrOptimum =
              mAttrsAndChildren.GetAttr(nsGkAtoms::optimum);
  if (!attrOptimum || attrOptimum->Type() != nsAttrValue::eDoubleValue) {
    return (min + max) / 2.0;
  }

  double optimum = attrOptimum->GetDoubleValue();

  if (optimum <= min) {
    return min;
  }

  return NS_MIN(optimum, max);
}

/*
 * XPCOM methods
 */

NS_IMETHODIMP
nsHTMLMeterElement::GetMin(double* aValue)
{
  *aValue = GetMin();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetMin(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::min, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetMax(double* aValue)
{
  *aValue = GetMax();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetMax(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::max, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetValue(double* aValue)
{
  *aValue = GetValue();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetValue(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::value, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetLow(double* aValue)
{
  *aValue = GetLow();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetLow(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::low, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetHigh(double* aValue)
{
  *aValue = GetHigh();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetHigh(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::high, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetOptimum(double* aValue)
{
  *aValue = GetOptimum();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetOptimum(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::optimum, aValue);
}

