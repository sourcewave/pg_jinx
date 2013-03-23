package org.sourcewave.jinx.example;

import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.sourcewave.jinx.fdw.ForeignDataWrapper;

public class FdwProperty extends ForeignDataWrapper {
  Map<String,Object> options;
  int foo = 0;
  Iterator<Entry<Object,Object>> iter;
  
  FdwProperty(Map<String,Object> x) {
    super();
    options = x;
    Logger.getAnonymousLogger().log(Level.WARNING, "options set: "+x);
  }
  
  @Override public void beginForeignScan() {
    iter = System.getProperties().entrySet().iterator();
  }
  
  
  @Override public String[] iterateForeignScan() {
    if (!iter.hasNext()) return null; 
    Entry<Object,Object> thisRow = iter.next();
    return new String[] { (String)thisRow.getKey(), (String)thisRow.getValue() };
  }

}
