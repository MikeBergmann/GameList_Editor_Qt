package de.mdb977.gamelisteditor;

import android.util.Log;
import android.view.KeyEvent;

import org.qtproject.qt.android.bindings.QtActivity;

// Qt maps no gamepad buttons (they arrive as Key_unknown), so re-send each as a key it does map.
public class GleActivity extends QtActivity
{
   private static int mapped( int aKeyCode )
   {
      switch ( aKeyCode )
      {
         case KeyEvent.KEYCODE_BUTTON_A: return KeyEvent.KEYCODE_ENTER;
         case KeyEvent.KEYCODE_BUTTON_B: return KeyEvent.KEYCODE_ESCAPE;
         case KeyEvent.KEYCODE_BUTTON_X: return KeyEvent.KEYCODE_F1;
         case KeyEvent.KEYCODE_BUTTON_Y: return KeyEvent.KEYCODE_F2;
         case KeyEvent.KEYCODE_BUTTON_L1: return KeyEvent.KEYCODE_PAGE_UP;
         case KeyEvent.KEYCODE_BUTTON_R1: return KeyEvent.KEYCODE_PAGE_DOWN;
         case KeyEvent.KEYCODE_BUTTON_START: return KeyEvent.KEYCODE_MENU;
         case KeyEvent.KEYCODE_BUTTON_SELECT: return KeyEvent.KEYCODE_F3;
         default: return 0;
      }
   }

   @Override
   public boolean dispatchKeyEvent( KeyEvent aEvent )
   {
      if ( KeyEvent.isGamepadButton( aEvent.getKeyCode() ) )
      {
         final int target = mapped( aEvent.getKeyCode() );
         Log.d( "GLE", "pad button " + aEvent.getKeyCode() + " action " + aEvent.getAction() + " -> " + target );
         if ( target != 0 )
            return super.dispatchKeyEvent( new KeyEvent( aEvent.getAction(), target ) );
      }
      return super.dispatchKeyEvent( aEvent );
   }
}
