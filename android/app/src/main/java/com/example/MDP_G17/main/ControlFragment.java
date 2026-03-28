package com.example.MDP_G17.main;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import com.example.MDP_G17.MainActivity;
import com.example.MDP_G17.R;
import java.util.Locale;

/**
 * ControlFragment is a Fragment class that displays the control buttons and timers for the robot in the
 * MainActivity.

 * It contains two timers for image recognition and fastest car simulation, and button listeners
 * to control the robot's movement.
 */
public class ControlFragment extends Fragment {
    private static final String TAG = "ControlFragment";
    //The MainActivity instance that contains this fragment.
    private final MainActivity mainActivity;

    private long imgRecTime, fastestCarTime;
    private SharedPreferences sharedPreferences;
    private ToggleButton imgRecBtn, fastestCarBtn;
    private TextView imgRecText, fastestCarText, robotStatusText;
    private GridMap gridMap;
    private int[] curCoord;
    private String direction;

    /**
     * Creates an instance of ControlFragment with the specified MainActivity instance.
     *
     * @param main the MainActivity instance that contains this fragment
     */
    public ControlFragment(MainActivity main) {
        this.mainActivity = main;
    }

    /**
     * The Handler used for timing the image recognition timer and fastest car timer.
     */
    public static Handler timerHandler = new Handler();

    /**
     * The Runnable for the image recognition timer.
     */
    public Runnable imgRecTimer = new Runnable() {
        @Override
        public void run() {
            long msTime = System.currentTimeMillis() - imgRecTime;
            int sTime = (int) (msTime / 1000);
            int minuteTime = sTime / 60;
            sTime = sTime % 60;

            if (! mainActivity.imgRecTimerFlag) {
                imgRecText.setText(String.format(Locale.US, "%02d:%02d", minuteTime, sTime));
                timerHandler.postDelayed(this, 500);
            }
        }
    };

    /**
     * The Runnable for the fastest car timer.
     */
    public Runnable fastestCarTimer = new Runnable() {
        @Override
        public void run() {
            long msTime = System.currentTimeMillis() - fastestCarTime;
            int sTime = (int) (msTime / 1000);
            int minuteTime = sTime / 60;
            sTime = sTime % 60;

            if (!mainActivity.fastestCarTimerFlag) {
                fastestCarText.setText(String.format(Locale.US,"%02d:%02d", minuteTime,
                        sTime));
                timerHandler.postDelayed(this, 500);
            }
        }
    };

    /**
     * Initializes the fragment.
     * @param savedInstanceState the saved instance state
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    /**
     * Creates and returns the view hierarchy associated with the fragment.
     * @param inflater           the LayoutInflater object that can be used to inflate any views in the fragment
     * @param container          the parent view that the fragment's UI should be attached to
     * @param savedInstanceState the saved instance state
     * @return the View for the fragment's UI, or null
     */
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // inflate
        View root = inflater.inflate(R.layout.activity_control, container, false);

        // get shared preferences
        this.sharedPreferences = requireActivity()
                .getSharedPreferences("Shared Preferences", Context.MODE_PRIVATE);

        // initialize all buttons and text views
        ImageButton forwardBtn = this.mainActivity.getUpBtn();
        ImageButton rightBtn = this.mainActivity.getRightBtn();
        ImageButton backBtn = this.mainActivity.getDownBtn();
        ImageButton leftBtn = this.mainActivity.getLeftBtn();
        ImageButton imgRecResetBtn = root.findViewById(R.id.exploreResetImageBtn2);
        ImageButton fastestCarResetBtn = root.findViewById(R.id.fastestResetImageBtn2);
        this.imgRecText = root.findViewById(R.id.exploreTimeTextView2);
        this.fastestCarText = root.findViewById(R.id.fastestTimeTextView2);
        this.imgRecBtn = root.findViewById(R.id.exploreToggleBtn2);
        this.fastestCarBtn = root.findViewById(R.id.fastestToggleBtn2);
        this.robotStatusText = this.mainActivity.getRobotStatusText();

        // default time 0
        this.fastestCarTime = 0;
        this.imgRecTime = 0;

        // need to get the gridMap to call the private methods
        this.gridMap = this.mainActivity.getGridMap();

        // button listeners
        forwardBtn.setOnClickListener(view -> {
            // only reacts when robot is placed on gridmap
            if (this.gridMap.getCanDrawRobot()) {
                this.curCoord = this.gridMap.getCurCoord();
                this.direction = this.gridMap.getRobotDirection();
                // handles translation based on existing direction
                switch (this.direction) {
                    case "up":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0], this.curCoord[1] + 1}, 0);
                        break;
                    case "left":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0] - 1, this.curCoord[1]}, 0);
                        break;
                    case "down":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0], this.curCoord[1] - 1}, 0);
                        break;
                    case "right":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0] + 1, this.curCoord[1]}, 0);
                        break;
                }
                this.mainActivity.refreshCoordinate();
                this.mainActivity.sendMessage("FORWARD");
                this.mainActivity.refreshCoordinate();
            } else {
                this.showToast("Please place robot on map to begin");
            }
        });

        backBtn.setOnClickListener(view -> {
            if (this.gridMap.getCanDrawRobot()) {
                this.curCoord = this.gridMap.getCurCoord();
                this.direction = this.gridMap.getRobotDirection();
                switch (this.direction) {
                    case "up":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0], this.curCoord[1] - 1}, 0);
                        break;
                    case "left":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0] + 1, this.curCoord[1]}, 0);
                        break;
                    case "down":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0], this.curCoord[1] + 1}, 0);
                        break;
                    case "right":
                        this.gridMap.moveRobot(new int[]{this.curCoord[0] - 1, this.curCoord[1]}, 0);
                        break;
                }
                this.mainActivity.sendMessage("BACKWARD");
                this.mainActivity.refreshCoordinate();
            } else {
                this.showToast("Please place robot on map to begin");
            }
        });

        leftBtn.setOnClickListener(view -> {
            if (this.gridMap.getCanDrawRobot()) {
                int[] cur = this.gridMap.getCurCoord();
                String currentDir = this.gridMap.getRobotDirection();
                int[] adj = new int[]{cur[0], cur[1]};

                switch (currentDir) {
                    case "up":    adj[0] += 0; adj[1] -= 1; break;
                    case "left":  adj[0] += 1; adj[1] += 0; break;
                    case "down":  adj[0] += 0; adj[1] += 1; break;
                    case "right": adj[0] -= 1; adj[1] += 0; break;
                }

                if (adj[0] >= 0 && adj[0] <= 19 && adj[1] >= 0 && adj[1] <= 19) {
                    this.gridMap.moveRobot(adj, 90);
                    this.mainActivity.refreshCoordinate();
                    this.mainActivity.sendMessage("LEFT");
                } else {
                    this.showToast("Cannot turn: Exceeds grid boundaries");
                }
            } else {
                this.showToast("Please place robot on map to begin");
            }
        });

        rightBtn.setOnClickListener(view -> {
            if (this.gridMap.getCanDrawRobot()) {
                int[] cur = this.gridMap.getCurCoord();
                String currentDir = this.gridMap.getRobotDirection();
                int[] adj = new int[]{cur[0], cur[1]};

                switch (currentDir) {
                    case "up":    adj[0] += 1; adj[1] += 0; break;
                    case "right": adj[0] += 0; adj[1] -= 1; break;
                    case "down":  adj[0] -= 1; adj[1] += 0; break;
                    case "left":  adj[0] += 0; adj[1] += 1; break;
                }

                if (adj[0] >= 0 && adj[0] <= 19 && adj[1] >= 0 && adj[1] <= 19) {
                    this.gridMap.moveRobot(adj, -90);
                    this.mainActivity.refreshCoordinate();
                    this.mainActivity.sendMessage("RIGHT");
                } else {
                    this.showToast("Cannot turn: Exceeds grid boundaries");
                }
            } else {
                this.showToast("Please place robot on map to begin");
            }
        });

        this.imgRecBtn.setOnClickListener(v -> {
            // changed from STOP to START (i.e. done with challenge)
            if (this.imgRecBtn.getText().equals("START")) {
                this.showToast("Image Recognition Completed");
                this.robotStatusText.setText(R.string.img_rec_stop);
                timerHandler.removeCallbacks(this.imgRecTimer);
            }
            // changed from START to STOP (i.e. started challenge)
            else if (this.imgRecBtn.getText().equals("STOP")) {
                this.mainActivity.imgRecTimerFlag = false;
                this.showToast("Image Recognition Started");
                this.mainActivity.sendMessage("START_C1");
                String getObsPos = this.gridMap.getAllObstacles();
                getObsPos = "OBS|" + getObsPos;
                this.mainActivity.sendMessage(getObsPos);
                this.robotStatusText.setText(R.string.img_rec_start);
                this.imgRecTime = System.currentTimeMillis();
                timerHandler.postDelayed(imgRecTimer, 0);
            }
        });

        this.fastestCarBtn.setOnClickListener(v -> {
            // changed from STOP to START (i.e., challenge completed)
            if (this.fastestCarBtn.getText().equals("START")) {
                this.showToast("Parking Stopped");
                this.robotStatusText.setText(R.string.fastest_car_stop);
                timerHandler.removeCallbacks(fastestCarTimer);
            }
            // changed from START to STOP (i.e., challenge started)
            else if (fastestCarBtn.getText().equals("STOP")) {
                this.showToast("Parking started");
                this.mainActivity.sendMessage("START_C2");
                this.mainActivity.fastestCarTimerFlag = false;
                this.robotStatusText.setText(R.string.fastest_car_start);
                this.fastestCarTime = System.currentTimeMillis();
                timerHandler.postDelayed(fastestCarTimer, 0);
            }
        });

        imgRecResetBtn.setOnClickListener(v -> {
            this.showToast("Resetting image recognition challenge timer...");
            this.imgRecText.setText(R.string.timer_default_val);
            this.robotStatusText.setText(R.string.robot_status_na);
            if (this.imgRecBtn.isChecked())
                this.imgRecBtn.toggle();
            timerHandler.removeCallbacks(imgRecTimer);
        });

        fastestCarResetBtn.setOnClickListener(view -> {
            this.showToast("Resetting fastest car challenge timer...");
            this.fastestCarText.setText(R.string.timer_default_val);
            this.robotStatusText.setText(R.string.robot_status_na);
            if (this.fastestCarBtn.isChecked()){
                this.fastestCarBtn.toggle();
            }
            timerHandler.removeCallbacks(fastestCarTimer);
        });

        return root;
    }

    /**
     * Method to display debug message
     * @param message The custom message shown in debugging
     */
    private void debugMessage(String message) {
        Log.d(TAG, message);
    }

    /**
     * Displays a toast with message on the UI
     * @param message The displayed message
     */
    private void showToast(String message) {
        Toast.makeText(getContext(), message, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onDestroy(){
        super.onDestroy();
    }
}