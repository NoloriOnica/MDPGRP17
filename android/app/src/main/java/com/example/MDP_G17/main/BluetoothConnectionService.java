package com.example.MDP_G17.main;

import android.annotation.SuppressLint;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.util.Log;
import android.widget.TextView;
import android.widget.Toast;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import com.example.MDP_G17.MainActivity;
import com.example.MDP_G17.R;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.UUID;

/**
 * Crash-course of the code:
 *
 * 1. {@code BluetoothConnectionService(Context context)} constructor is called when an
 * object of this class is created.
 *
 * 2. {@code startAcceptThread()} is called from the constructor.
 *
 * 3. {@code AcceptThread()} constructor is called inside {@code startAcceptThread()}.
 *
 * 4. {@code run()} method of {@code AcceptThread()} is executed when
 * {@code mInsecureAcceptThread.start()} is called inside {@code startAcceptThread()}.
 *
 * 5. {@code connected()} is called inside {@code AcceptThread()} when a socket is successfully accepted.
 *
 * 6. {@code startClientThread(BluetoothDevice device, UUID uuid)} is called when the user selects a
 * Bluetooth device to connect to.
 *
 * 7. {@code ConnectThread()} constructor is called inside
 * {@code startClientThread(BluetoothDevice device, UUID uuid)}.
 *
 * 8. {@code run()} method of {@code ConnectThread()} is executed when {@code mConnectThread.start()} is called
 * inside {@code startClientThread(BluetoothDevice device, UUID uuid)}.
 *
 * 9. {@code connected()} is called inside {@code ConnectThread()} when a socket is successfully connected.
 *
 * 10. {@code cancel()} methods of {@code AcceptThread()} or {@code ConnectThread()} are called when a socket is
 * closed or the user cancels the Bluetooth connection.
 *
 * 11. {@code write(byte[] out)} is called to write data to the connected device through the Bluetooth socket.
 *
 * 12. {@code read()} method of {@code ConnectThread()} is called to read data from the connected device
 * through the Bluetooth socket.
 *
 * 13. {@code stop()} method of {@code ConnectThread()} is called to stop the thread and close the
 * Bluetooth socket.
 *
 *
 * On start, AcceptThread is initiated (runs in the background and listens for incoming Bluetooth connection requests).
 * When a request is received, it creates a Bluetooth socket
 * If RPI connects to Android, then it is AcceptThread -> ConnectedThread
 * If Android intiates connection, entry point is {@code startClientThread(BluetoothDevice device, UUID uuid)},
 * then it is ConnectThread -> ConnectedThread
 */

public class BluetoothConnectionService {
    private static final String TAG = "DebuggingTag";
    private static final String appName = "MDP_G17";
    public static final UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private final BluetoothAdapter mBluetoothAdapter;
    Context mContext;
    private AcceptThread mInsecureAcceptThread;
    private ConnectThread mConnectThread;
    private BluetoothDevice mDevice;
    private UUID deviceUUID;
    ProgressDialog mProgressDialog;
    Intent connectionStatus;
    public static boolean BluetoothConnectionStatus=false;
    private static ConnectedThread mConnectedThread;

    /**
     * Constructor, intiializes the BT adapter and starts AcceptThread.
     * @param context The current state of the application
     */
    public BluetoothConnectionService(Context context) {
        this.mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        this.mContext = context;
        this.startAcceptThread();
    }

    /**
     * This class is used to create a server socket and listen for incoming connections
     * from other devices. When a connection is established, it calls the {@code connected()} method to
     * create a connected thread.
     */
    private class AcceptThread extends Thread{
        // Used to listen for incoming connections.
        private final BluetoothServerSocket ServerSocket;

        @SuppressLint("MissingPermission")
        public AcceptThread() {
            BluetoothServerSocket tmp = null;
            // starts listening for incoming connections
            try {
                tmp = mBluetoothAdapter.listenUsingInsecureRfcommWithServiceRecord(appName, myUUID);
            } catch (IOException e){
                e.printStackTrace();
            }
            this.ServerSocket = tmp;
        }

        /**
         * The main method of the AcceptThread. It runs continuously and listens
         * for incoming connections. Once a connection is accepted,
         * it calls the {@code connected()} method.
         */
        public void run(){
            BluetoothSocket socket = null;
            try {
                socket = ServerSocket.accept();
            } catch (IOException e){
                e.printStackTrace();
            }

            // connection accepted
            if (socket != null){
                connected(socket, socket.getRemoteDevice());
            }
        }

        /**
         * A method that cancels the ServerSocket and stops the AcceptThread.
         */
        public void cancel(){
            try{
                ServerSocket.close();
            } catch(IOException e){
                e.printStackTrace();
            }
        }
    }

    /**
     * A private class that extends Thread. It initiates an outgoing Bluetooth connection.
     */
    private class ConnectThread extends Thread{
        // A BluetoothSocket object that represents the outgoing Bluetooth connection.
        private BluetoothSocket mSocket;

        /**
         * A constructor for the ConnectThread class.
         * It creates a new BluetoothSocket and attempts to connect to the remote device.
         * @param device The remote device
         * @param u The UUID of the remote device
         */
        public ConnectThread(BluetoothDevice device, UUID u){
            mDevice = device;
            deviceUUID = u;
        }

        /**
         * The main method of the ConnectThread. It attempts to connect to the remote device.
         * If the connection is successful, it calls the {@code connected()} method.
         */
        @SuppressLint("MissingPermission")
        public void run(){
            BluetoothSocket tmp = null;
            try {
                tmp = mDevice.createRfcommSocketToServiceRecord(deviceUUID);
            } catch (IOException e) {
                e.printStackTrace();
            }
            mSocket= tmp;
            mBluetoothAdapter.cancelDiscovery();

            try {
                mSocket.connect();
                connected(mSocket,mDevice);
            } catch (IOException e) {
                try {
                    mSocket.close();
                } catch (IOException e1) {
                    e.printStackTrace();
                }

                try {
                    BluetoothPopUp mBluetoothPopUpActivity = (BluetoothPopUp) mContext;
                    mBluetoothPopUpActivity.runOnUiThread(() -> Toast.makeText(mContext,
                            "Failed to connect to the Device.", Toast.LENGTH_SHORT).show());
                } catch (Exception z) {
                    z.printStackTrace();
                }

            }
            try {
                mProgressDialog.dismiss();
            } catch (NullPointerException e){
                e.printStackTrace();
            }
        }

        //Closes the BluetoothSocket and stops the ConnectThread.
        public void cancel(){
            try{
                mSocket.close();
            } catch(IOException e){
                e.printStackTrace();
            }
        }
    }

    //A method that starts the AcceptThread. If the ConnectThread is running, it is stopped.
    public synchronized void startAcceptThread(){
        //Cancel any thread attempting to make a connection
        if (this.mConnectThread != null){
            this.mConnectThread.cancel();
            this.mConnectThread = null;
        }

        // If accept thread is null we want to start a new one
        if (this.mInsecureAcceptThread == null){
            this.mInsecureAcceptThread = new AcceptThread();
            this.mInsecureAcceptThread.start();
        }
    }

    /**
     * A method that starts the ConnectThread to initiate an outgoing Bluetooth connection.
     * @param device The remote device
     * @param uuid The UUID of the remote device
     */
    public void startClientThread(BluetoothDevice device, UUID uuid){
        try {
            this.mProgressDialog = ProgressDialog.show(this.mContext, "Connecting Bluetooth", "Please Wait...", true);
        } catch (Exception e) {
            e.printStackTrace();
        }
        // starts the ConnectThread
        this.mConnectThread = new ConnectThread(device, uuid);
        this.mConnectThread.start();
    }

    private class ConnectedThread extends Thread{
        private final BluetoothSocket mSocket;
        private final InputStream inStream;
        private final OutputStream outStream;
        private boolean stopThread = false;

        @SuppressLint("MissingPermission")
        public ConnectedThread(BluetoothSocket socket) {
            Log.d(TAG, "ConnectedThread: Starting.");

            connectionStatus = new Intent("ConnectionStatus");
            connectionStatus.putExtra("Status", "connected");
            connectionStatus.putExtra("Device", mDevice);
            LocalBroadcastManager.getInstance(mContext).sendBroadcast(connectionStatus);
            BluetoothConnectionStatus = true;

            TextView status = MainActivity.getBluetoothStatus();
            status.setText(R.string.bt_connected);
            status.setTextColor(Color.GREEN);

            TextView device = MainActivity.getConnectedDevice();
            device.setText(mDevice.getName());

            this.mSocket = socket;
            InputStream tmpIn = null;
            OutputStream tmpOut = null;

            try {
                tmpIn = mSocket.getInputStream();
                tmpOut = mSocket.getOutputStream();
            } catch (IOException e) {
                e.printStackTrace();
            }

            inStream = tmpIn;
            outStream = tmpOut;
        }

        public void run(){
            byte[] buffer = new byte[1024];
            int bytes;
            StringBuilder messageBuffer = new StringBuilder();

            while (true){
                try {
                    bytes = inStream.read(buffer);
                    String incomingmessage = new String(buffer, 0, bytes);
                    Log.d(TAG, "InputStream: "+ incomingmessage);

                    messageBuffer.append(incomingmessage);

                    // Check if the buffer contains the delimiter
                    int delimiterIndex = messageBuffer.indexOf("\n");
                    if (delimiterIndex != -1) {
                        // Split the buffer contents and process each message
                        String[] messages = messageBuffer.toString().split("\n");
                        for (String message : messages) {
                            // Send broadcast for each incoming message
                            Intent incomingMessageIntent = new Intent("incomingMessage");
                            incomingMessageIntent.putExtra("receivedMessage", message);

                            LocalBroadcastManager.getInstance(mContext).sendBroadcast(incomingMessageIntent);
                        }

                        // Reset the message buffer
                        messageBuffer = new StringBuilder();
                    }
                } catch (IOException e) {
                    Log.e(TAG, "Error reading input stream. "+e.getMessage());

                    connectionStatus = new Intent("ConnectionStatus");
                    connectionStatus.putExtra("Status", "disconnected");
                    TextView status = MainActivity.getBluetoothStatus();
                    status.setText(R.string.bt_disconnect);
                    status.setTextColor(Color.RED);
                    connectionStatus.putExtra("Device", mDevice);
                    LocalBroadcastManager.getInstance(mContext).sendBroadcast(connectionStatus);
                    BluetoothConnectionStatus = false;

                    break;
                }
            }
        }

        public void write(byte[] bytes){
            String text = new String(bytes, Charset.defaultCharset());
            Log.d(TAG, "write: Writing to output stream: "+text);
            try {
                outStream.write(bytes);
            } catch (IOException e) {
                Log.e(TAG, "Error writing to output stream. "+e.getMessage());
            }
        }

        public void cancel(){
            Log.d(TAG, "cancel: Closing Client Socket");
            try{
                this.stopThread = true;
                mSocket.close();
            } catch(IOException e){
                Log.e(TAG, "cancel: Failed to close ConnectThread mSocket " + e.getMessage());
            }
        }
    }

    /**
     * A method that initializes and starts the ConnectedThread to manage the Bluetooth connection.
     * @param mSocket The BT socket
     * @param device The device connected
     */
    private void connected(BluetoothSocket mSocket, BluetoothDevice device) {
        mDevice =  device;
        // stops the AcceptThread when received request
        if (mInsecureAcceptThread != null) {
            mInsecureAcceptThread.cancel();
            mInsecureAcceptThread = null;
        }

        mConnectedThread = new ConnectedThread(mSocket);
        mConnectedThread.start();
    }

    public static void write(byte[] out){
        ConnectedThread tmp;

        Log.d(TAG, "write: Write is called." );
        mConnectedThread.write(out);
    }
}