package com.example.MDP_G17.main;

import android.app.DialogFragment;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.example.MDP_G17.MainActivity;
import com.example.MDP_G17.R;

public class SaveMapFragment extends DialogFragment {

    private static final String TAG = "SaveMapFragment";
    private SharedPreferences.Editor editor;

    SharedPreferences sharedPreferences;

    Button saveBtn, cancelBtn;
    String map;
    View rootView;

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, Bundle savedInstanceState) {
        showLog("Entering onCreateView");
        rootView = inflater.inflate(R.layout.activity_save_map, container, false);
        super.onCreate(savedInstanceState);

        getDialog().setTitle("Save Map");
        sharedPreferences = getActivity().getSharedPreferences("Shared Preferences", Context.MODE_PRIVATE);
        editor = sharedPreferences.edit();

        saveBtn = rootView.findViewById(R.id.saveBtn);
        cancelBtn = rootView.findViewById(R.id.cancelSaveMapBtn);

        map = sharedPreferences.getString("mapChoice","");

        if (savedInstanceState != null)
            map = savedInstanceState.getString("mapChoice");

        final Spinner spinner = (Spinner) rootView.findViewById(R.id.mapDropdownSpinner);

        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(getActivity(),
                R.array.save_map_array, android.R.layout.simple_spinner_item);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);

        saveBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                showLog("Clicked saveBtn");
                map = spinner.getSelectedItem().toString();

                GridMap gridMap = ((MainActivity)getActivity()).getGridMap();

                // 1. Send the Raspberry Pi its special format over the Bluetooth bridge
                String rpiMessage = gridMap.getAllObstacles();
                ((MainActivity)getActivity()).sendMessage("OBS|" + rpiMessage);

                // 2. Forge the perfectly formatted internal string for our Local Memory
                StringBuilder localSaveData = new StringBuilder();
                for (int i = 0; i < gridMap.getObstacleCoord().size(); i++) {
                    int[] obs = gridMap.getObstacleCoord().get(i);
                    int x = obs[0];
                    int y = obs[1];
                    int id = obs[2];
                    String bearing = gridMap.getImageBearing(x, y);
                    char dir = (bearing != null && !bearing.isEmpty()) ? bearing.charAt(0) : 'N';

                    // Append in the exact format the Loader expects: x,y,dir,id
                    localSaveData.append(x).append(",")
                            .append(y).append(",")
                            .append(dir).append(",")
                            .append(id);

                    if (i < gridMap.getObstacleCoord().size() - 1) {
                        localSaveData.append("|");
                    }
                }

                // 3. Commit the perfectly formatted memory to the vault
                editor.putString("mapChoice", map);
                editor.putString(map, localSaveData.toString());
                editor.commit();

                Toast.makeText(getActivity(), "Saving " + map, Toast.LENGTH_SHORT).show();
                showLog("Exiting saveBtn");
                getDialog().dismiss();
            }
        });

        cancelBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                showLog("Clicked cancelDirectionBtn");
                getDialog().dismiss();
            }
        });
        showLog("Exiting onCreateView");
        return rootView;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        showLog("Entering onSaveInstanceState");
        super.onSaveInstanceState(outState);
        saveBtn = rootView.findViewById(R.id.saveBtn);
        showLog("Exiting onSaveInstanceState");
        outState.putString(TAG, map);
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        showLog("Entering onDismiss");
        super.onDismiss(dialog);
        showLog("Exiting onDismiss");
    }

    private void showLog(String message) {
        Log.d(TAG, message);
    }
}