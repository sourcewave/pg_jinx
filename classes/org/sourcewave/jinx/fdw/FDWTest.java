package org.sourcewave.jinx.fdw;

import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

// I need a constructor that returns an object given a map
public class FDWTest extends ForeignDataWrapper {
  Map<String,Object> options;
  int foo = 0;
  
  FDWTest(Map<String,Object> x) {
    super();
    options = x;
    Logger.getAnonymousLogger().log(Level.WARNING, "options set: "+x);
  }
  
  @Override public void beginForeignScan() {
    foo = 0;
  }
  
  
  @Override public Object[] iterateForeignScan() {
    if (foo > 10) return null;
    foo += 1;
    return new Object[] { foo * foo, foo+" to "+foo, Math.exp(foo) };
  }

}
  