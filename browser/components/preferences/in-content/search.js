/*   ***** BEGIN LICENSE BLOCK *****
	   - This Source Code Form is subject to the terms of the Mozilla Public License,
	   - v. 2.0. If a copy of the MPL was not distributed with this file,
	   - You can obtain one at http://mozilla.org/MPL/2.0/.
	   -
	   - ***** END LICENSE BLOCK ***** */
	   
function search(query, attribute){
  let categories = document.querySelectorAll("[data-category]");
  for(var i=0; i<categories.length; i++){
    let element = categories.item(i);
    let attributeValue = element.getAttribute(attribute);
    if(attributeValue==query.toLowerCase()){
      element.hidden=false;
    } else element.hidden=true;
  }
}