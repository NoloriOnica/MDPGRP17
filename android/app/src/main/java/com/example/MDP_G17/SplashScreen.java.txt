package com.example.MDP_G17;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class SplashScreen extends Activity {
    Thread handler;
    @Override
    protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_splash_screen);
        handler = new Thread(){
            public void run(){
                try{
                    sleep(2000);
                    Intent main = new Intent(SplashScreen.this, MainActivity.class);
                    startActivity(main);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }finally{
                    finish();
                }
            }
        };
        handler.start();
    }
}
