/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Taras Glek <tglek@mozilla.com>
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

/**
 * This file lists Telemetry histograms collected by Firefox.
 *  Format is HISTOGRAM(id, histogram name, minium, maximum, bucket count,
 * histogram kind, human-readable description for about:telemetry)
 */

HISTOGRAM(CYCLE_COLLECTOR, "nsCycleCollector::Collect (ms)", 1, 10000, 50, EXPONENTIAL, "Time spent on cycle collection")
HISTOGRAM(TELEMETRY_PING, "Telemetry.ping (ms)", 1, 3000, 10, EXPONENTIAL, "Telemetry submission lag")
HISTOGRAM(TELEMETRY_SUCCESS, "Telemetry.success (No, Yes)", 0, 1, 2, BOOLEAN,  "Success rate of telemetry submissions")
HISTOGRAM(MEMORY_JS_GC_HEAP, "Memory::explicit/js/gc-heap (MB)", 1024, 512 * 1024, 10, EXPONENTIAL, "Memory used by the JavaScript GC")
HISTOGRAM(MEMORY_RESIDENT, "Memory::resident (MB)", 32 * 1024, 1024 * 1024, 10, EXPONENTIAL, "Resident memory reported by OS")
HISTOGRAM(MEMORY_LAYOUT_ALL, "Memory::explicit/layout/all (MB)", 1024, 64 * 1024, 10, EXPONENTIAL, "Memory reported used by layout")
