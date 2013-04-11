package org.sourcewave.jinx;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Iterator;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.sourcewave.jinx.fdw.ForeignDataWrapper;

public class FdwJars extends ForeignDataWrapper {
    Map<String,Object> options;
    Iterator<Path> dirStream;
    
    FdwJars(Map<String,Object> x) {
      super();
      options = x;
      Logger.getAnonymousLogger().log(Level.WARNING, "options set: "+x);
    }
    
    @Override public void beginForeignScan() {
        try { 
          dirStream = Files.newDirectoryStream(Paths.get(System.getProperty("jinx.extDir")), "*.jar").iterator();
        } catch(IOException iox) {
          dirStream = null;
        }
    }
    
    @Override public String[] iterateForeignScan() {
      if (dirStream == null || !dirStream.hasNext()) return null; 
      Path thisRow = dirStream.next();
      return new String[] { (String)thisRow.getFileName().toString(), null };
    }

}
