package com.example.MDP_G17.main;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import com.example.MDP_G17.MainActivity;
import com.example.MDP_G17.R;

import java.nio.charset.Charset;


/**
 * A fragment representing the Bluetooth chat UI.
 */
public class BluetoothChatFragment extends Fragment {

    /**
     * The tag used for logging debug messages.
     */
    private static final String TAG = "CommsFragment";

    /**
     * The shared preferences used for storing messages.
     */
    private SharedPreferences sharedPreferences;

    /**
     * The text view for displaying received messages.
     */
    private TextView messageReceivedTextView;

    /**
     * The text box for typing messages to send.
     */
    private EditText typeBoxEditText;

    /**
     * The main activity that this fragment is attached to.
     */
    private final MainActivity mainActivity;

    /**
     * Constructs a new instance of the BluetoothChatFragment.
     *
     * @param main the main activity that this fragment is attached to.
     */
    public BluetoothChatFragment(MainActivity main) {
        this.mainActivity = main;
    }

    /**
     * Called when the fragment is being created.
     *
     * @param savedInstanceState the saved instance state.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LocalBroadcastManager.getInstance(this.requireContext())
                .registerReceiver(this.mReceiver, new IntentFilter("incomingMessage"));
    }

    /**
     * Called when the fragment's UI is being created.
     *
     * @param inflater           the layout inflater.
     * @param container          the view group container.
     * @param savedInstanceState the saved instance state.
     * @return the root view for the fragment's UI.
     */
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {

        // Inflate the fragment's UI layout
        View root = inflater.inflate(R.layout.activity_comms, container, false);

        // Get a reference to the send button
        ImageButton send = root.findViewById(R.id.messageButton);

        // Get references to the message text view and type box edit text
        this.messageReceivedTextView = root.findViewById(R.id.messageReceivedTitleTextView);
        this.messageReceivedTextView.setMovementMethod(new ScrollingMovementMethod());
        this.typeBoxEditText = root.findViewById(R.id.typeBoxEditText);

        // Get a reference to the shared preferences used for storing messages
        this.sharedPreferences = this.requireActivity()
                .getSharedPreferences("Shared Preferences", Context.MODE_PRIVATE);

        // Set a click listener for the send button
        send.setOnClickListener(view -> {
            String sentText = "" + this.typeBoxEditText.getText().toString();
            SharedPreferences.Editor editor = sharedPreferences.edit();
            editor.putString("message", sharedPreferences
                    .getString("message", "") + '\n' + sentText);
            editor.apply();
            this.messageReceivedTextView.append(sentText + "\n");
            this.typeBoxEditText.setText("");

            if (BluetoothConnectionService.BluetoothConnectionStatus) {
                byte[] bytes = sentText.getBytes(Charset.defaultCharset());
                BluetoothConnectionService.write(bytes);
            }
        });
        return root;
    }

    /**
     * Returns the text view for displaying received messages.
     *
     * @return the text view for displaying received messages.
     */
    public TextView getMessageReceivedTextView() {
        return this.messageReceivedTextView;
    }

    /**
     * The broadcast receiver for incoming messages.
     */
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String text = intent.getStringExtra("receivedMessage");
            messageReceivedTextView.append(text + "\n");
        }
    };

    /**
     * Logs a debug message.
     *
     * @param debugContext the message to log.
     */
    private void debugMessage(String debugContext) {
        Log.d(BluetoothChatFragment.TAG, debugContext);
    }
}