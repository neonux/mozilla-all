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
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
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



#include "MaemoLocationProvider.h"
#include "nsGeolocation.h"

NS_IMPL_ISUPPORTS1(MaemoLocationProvider, nsIGeolocationProvider)

MaemoLocationProvider::MaemoLocationProvider()
: mGPSDevice(nsnull), mLocationCallbackHandle(0), mHasSeenLocation(PR_FALSE)
{
}

MaemoLocationProvider::~MaemoLocationProvider()
{
}

void location_changed (LocationGPSDevice *device, gpointer userdata)
{
  MaemoLocationProvider* provider = (MaemoLocationProvider*) userdata;
  nsRefPtr<nsGeolocation> somewhere = new nsGeolocation(device->fix->latitude,
                                                        device->fix->longitude,
                                                        device->fix->altitude,
                                                        device->fix->eph,
                                                        device->fix->epv,
                                                        device->fix->time);
  provider->Update(somewhere);
}

NS_IMETHODIMP MaemoLocationProvider::Startup()
{
  if (!mGPSDevice)
  {
    // if we are already started, don't do anything
    memset(&mGPSBT, 0, sizeof(gpsbt_t));
    int result = gpsbt_start(NULL, 0, 0, 0, NULL, 0, 0, &mGPSBT);
    if (result <0)
      return NS_ERROR_NOT_AVAILABLE;
    
    mGPSDevice = (LocationGPSDevice*) g_object_new (LOCATION_TYPE_GPS_DEVICE, NULL);
    mLocationCallbackHandle = g_signal_connect (mGPSDevice, "changed", G_CALLBACK (location_changed), this->mCallback);
  }
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::IsReady(PRBool *_retval NS_OUTPARAM)
{
  *_retval = mHasSeenLocation;
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Watch(nsIGeolocationUpdate *callback)
{
  mCallback = callback; // weak ref
  return NS_OK;
}

/* readonly attribute nsIDOMGeolocation currentLocation; */
NS_IMETHODIMP MaemoLocationProvider::GetCurrentLocation(nsIDOMGeolocation * *aCurrentLocation)
{
  NS_IF_ADDREF(*aCurrentLocation = mLastLocation);
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Shutdown()
{
  if (mGPSDevice && mLocationCallbackHandle) {
    g_signal_handler_disconnect(mGPSDevice, mLocationCallbackHandle);
    g_object_unref(mGPSDevice);
    gpsbt_stop(&mGPSBT);
    mLocationCallbackHandle = 0;
    mGPSDevice = nsnull;
    mHasSeenLocation = PR_FALSE;
  }
  return NS_OK;
}

void MaemoLocationProvider::Update(nsIDOMGeolocation* aLocation)
{
  mHasSeenLocation = PR_TRUE;
  mLastLocation = aLocation;
  if (mCallback)
    mCallback->Update(aLocation);
}
