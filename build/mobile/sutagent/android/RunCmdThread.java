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
 * The Original Code is Android SUTAgent code.
 *
 * The Initial Developer of the Original Code is
 * Bob Moss.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bob Moss <bmoss@mozilla.com>
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

package com.mozilla.SUTAgentAndroid.service;

import java.io.IOException;
import java.io.InputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.List;

import com.mozilla.SUTAgentAndroid.R;
import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;

public class RunCmdThread extends Thread
    {
    private ServerSocket SvrSocket = null;
    private Socket socket    = null;
    private Handler handler = null;
    boolean bListening    = true;
    boolean bNetError = false;
    List<CmdWorkerThread> theWorkers = new ArrayList<CmdWorkerThread>();
    android.app.Service    svc = null;

    public RunCmdThread(ServerSocket socket, android.app.Service service, Handler handler)
        {
        super("RunCmdThread");
        this.SvrSocket = socket;
        this.svc = service;
        this.handler = handler;
        }

    public void StopListening()
        {
        bListening = false;
        }

    public void run() {
        try {
            int    nIterations = 0;

            SvrSocket.setSoTimeout(5000);
            while (bListening)
                {
                try
                    {
                    socket = SvrSocket.accept();
                    CmdWorkerThread theWorker = new CmdWorkerThread(this, socket);
                    theWorker.start();
                    theWorkers.add(theWorker);
                    }
                catch (SocketTimeoutException toe)
                    {
                    if (++nIterations > 60)
                        {
                        nIterations = 0;
                        String sRet = SendPing("www.mozilla.org");
                        if (sRet.contains("3 received"))
                            handler.post(new doCancelNotification());
                        else
                            handler.post(new doSendNotification("SUTAgent - Network Connectivity Error", sRet));
                        sRet = null;
                        }
                    continue;
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    continue;
                    }
                }

            int nNumWorkers = theWorkers.size();
            for (int lcv = 0; lcv < nNumWorkers; lcv++)
                {
                if (theWorkers.get(lcv).isAlive())
                    {
                    theWorkers.get(lcv).StopListening();
                    while(theWorkers.get(lcv).isAlive())
                        ;
                    }
                }

            theWorkers.clear();

            SvrSocket.close();

            svc.stopSelf();

//            SUTAgentAndroid.me.finish();
            }
        catch (IOException e)
            {
            e.printStackTrace();
            }
        return;
        }

    private String SendPing(String sIPAddr)
        {
        Process    pProc;
        String sRet = "";
        String [] theArgs = new String [4];
        boolean bStillRunning = true;
        int    nBytesOut = 0;
        int nBytesErr = 0;
        int nBytesRead = 0;
        byte[] buffer = new byte[1024];

        theArgs[0] = "ping";
        theArgs[1] = "-c";
        theArgs[2] = "3";
        theArgs[3] = sIPAddr;

        try
            {
            pProc = Runtime.getRuntime().exec(theArgs);

            InputStream sutOut = pProc.getInputStream();
            InputStream sutErr = pProc.getErrorStream();

            while (bStillRunning)
                {
                try
                    {
                    if ((nBytesOut = sutOut.available()) > 0)
                        {
                        if (nBytesOut > buffer.length)
                            {
                            buffer = null;
                            System.gc();
                            buffer = new byte[nBytesOut];
                            }
                        nBytesRead = sutOut.read(buffer, 0, nBytesOut);
                        if (nBytesRead == -1)
                            bStillRunning = false;
                        else
                            {
                            String sRep = new String(buffer,0,nBytesRead).replace("\n", "\r\n");
                            sRet += sRep;
                            sRep = null;
                            }
                        }

                    if ((nBytesErr = sutErr.available()) > 0)
                        {
                        if (nBytesErr > buffer.length)
                            {
                            buffer = null;
                            System.gc();
                            buffer = new byte[nBytesErr];
                            }
                        nBytesRead = sutErr.read(buffer, 0, nBytesErr);
                        if (nBytesRead == -1)
                            bStillRunning = false;
                        else
                            {
                            String sRep = new String(buffer,0,nBytesRead).replace("\n", "\r\n");
                            sRet += sRep;
                            sRep = null;
                            }
                        }

                    bStillRunning = (IsProcRunning(pProc) || (sutOut.available() > 0) || (sutErr.available() > 0));
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    }

                if ((bStillRunning == true) && (nBytesErr == 0) && (nBytesOut == 0))
                    {
                    try {
                        sleep(2000);
                        }
                    catch (InterruptedException e) {
                        e.printStackTrace();
                        }
                    }
                }

            pProc.destroy();
            pProc = null;
            }
        catch (IOException e)
            {
            sRet = e.getMessage();
            e.printStackTrace();
            }

        return (sRet);
        }

    private boolean IsProcRunning(Process pProc)
        {
        boolean bRet = false;
        @SuppressWarnings("unused")
        int nExitCode = 0;

        try
            {
            nExitCode = pProc.exitValue();
            }
        catch (IllegalThreadStateException z)
            {
            bRet = true;
            }
        catch (Exception e)
            {
            e.printStackTrace();
            }

        return(bRet);
        }

    private void SendNotification(String tickerText, String expandedText)
        {
        NotificationManager notificationManager = (NotificationManager)svc.getSystemService(Context.NOTIFICATION_SERVICE);

//        int icon = android.R.drawable.stat_notify_more;
//        int icon = R.drawable.ic_stat_first;
//        int icon = R.drawable.ic_stat_second;
//        int icon = R.drawable.ic_stat_neterror;
        int icon = R.drawable.ateamlogo;
        long when = System.currentTimeMillis();

        Notification notification = new Notification(icon, tickerText, when);

        notification.flags |= (Notification.FLAG_INSISTENT | Notification.FLAG_AUTO_CANCEL);
        notification.defaults |= Notification.DEFAULT_SOUND;
        notification.defaults |= Notification.DEFAULT_VIBRATE;
        notification.defaults |= Notification.DEFAULT_LIGHTS;

        Context context = svc.getApplicationContext();

        // Intent to launch an activity when the extended text is clicked
        Intent intent2 = new Intent(svc, SUTAgentAndroid.class);
        PendingIntent launchIntent = PendingIntent.getActivity(context, 0, intent2, 0);

        notification.setLatestEventInfo(context, tickerText, expandedText, launchIntent);

        notificationManager.notify(1959, notification);
        }

    private void CancelNotification()
        {
        NotificationManager notificationManager = (NotificationManager)svc.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(1959);
        }

    class doCancelNotification implements Runnable
        {
        public void run()
            {
            CancelNotification();
            }
        };

    class doSendNotification implements Runnable
        {
        private String sTitle = "";
        private String sBText = "";

        doSendNotification(String sTitle, String sBodyText)
            {
            this.sTitle = sTitle;
            this.sBText = sBodyText;
            }

        public void run()
            {
            SendNotification(sTitle, sBText);
            }
        };
}