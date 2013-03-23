package org.sourcewave.jinx.example;

import java.io.IOException;
import org.sourcewave.jinx.Trigger;

public class GravatarTrigger extends Trigger {
  // this is the Trigger
  @Override public void invoke() throws Exception {
    String s = Example.getGravatarURL((String)OLD[0]);
    OLD[1] = s;
  }
  
  

  
}
