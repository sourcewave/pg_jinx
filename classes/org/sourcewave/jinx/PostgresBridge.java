package org.sourcewave.jinx;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.sql.SQLException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.logging.Formatter;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.LogRecord;

public class PostgresBridge {
  public static List<String> jarList() throws IOException {
    DirectoryStream<Path> st = Files.newDirectoryStream(Paths.get(System.getProperty("jinx.extDir")), "*.jar");
    List<String> ls = new LinkedList<String>();
    for(Path p : st) {
      ls.add(p.getFileName().toString());
    }
    return ls;
  }
  public static List<String> install_jar(byte[] b, String s) throws IOException {
    if (!s.endsWith(".jar")) s+= ".jar";
    
    Path pd = Paths.get(System.getProperty("jinx.extDir"));
    pd = Files.createDirectories(pd);
    Files.write( Paths.get(pd.toString(),s), b);
    return jarList();
  }
  
  public static List<String> remove_jar(String s) throws IOException {
    if (!s.endsWith(".jar")) s+= ".jar";
    Path p = Paths.get(System.getProperty("jinx.extDir"),s);
    Files.delete(p);
    return jarList();
  }
  
  static {
    init();
  }
  
  public static void init() {
    PgLogger.init();
  }
  
  public static native String[] getUser();
  
  public static native void log(int logLevel, String str);

  public static native long _formTuple(long _this, Object[] values) throws SQLException;

  // from Relation
  public static native long _modifyTuple(long pointer, long original, int[] fieldNumbers, Object[] values) throws SQLException;
  
  public static native long _prepare(long threadId, String statement, int[] argTypes)
    throws SQLException;
  
  // portal
  public static native int _fetch(long pointer, long threadId, boolean forward, int count)
  throws SQLException;

  public static native int _move(long pointer, long threadId, boolean forward, int count) throws SQLException;


  public static Object execute(int num, String cmd, Object...parameters) throws ServerException {
    return _execute(num, cmd, parameters);
  }
  public static native Object _execute(int num, String cmd,  Object[] parameters) throws ServerException;

  public static native int sp_start() throws ServerException;
  public static native void sp_commit() throws ServerException;
  public static native void sp_rollback() throws ServerException;
  
  
  public enum PgLogLevel {
    DEBUG5(10), DEBUG4(11), DEBUG3(12), DEBUG2(13), DEBUG1(14), LOG(15), INFO(17), NOTICE(18),
    WARNING(19), ERROR(20), FATAL(21), PANIC(22);
    int pglevel;
    PgLogLevel(int n) { pglevel=n; }
    static Map<Level, PgLogLevel> map;
    static {
      map = new HashMap<Level,PgLogLevel>();
      map.put(Level.SEVERE, ERROR);
      map.put(Level.WARNING, WARNING);
      map.put(Level.INFO,  INFO);
      map.put(Level.FINE, DEBUG1);
      map.put(Level.FINER,DEBUG2);
      map.put(Level.FINEST, DEBUG3);
    }
    public static PgLogLevel fromLevel(Level l) {
      PgLogLevel ll = map.get(l);
      return ll == null ? LOG : ll;
    }
    
  }
  
  public static class PgLogger extends Handler {
    
    @Override public void publish(LogRecord record) {
      log(PgLogLevel.fromLevel(record.getLevel()).pglevel, getFormatter().format(record));
    }

    public PgLogger() { setFormatter(new PgLogFormatter()); }
    @Override public void flush() { }
    @Override public void close() throws SecurityException { }

    public static void init() {
      Properties props = new Properties();
      props.setProperty("handlers", PgLogger.class.getName());
      ByteArrayOutputStream po = new ByteArrayOutputStream();
      try {
        props.store(po, null);
        LogManager.getLogManager().readConfiguration(
          new ByteArrayInputStream(po.toByteArray()));
      }
      catch(IOException e) { }
    }
  }
  
  public static class PgLogFormatter extends Formatter {
    @Override public synchronized String format(LogRecord record) {
      StringBuffer sb = new StringBuffer();
      SimpleDateFormat sdf = new SimpleDateFormat("dd MMM yy:HH:mm:ss ");
      sb.append( sdf.format( new Date(record.getMillis())));
      sb.append( record.getSourceClassName() == null ? record.getLoggerName() : record.getSourceClassName());
      sb.append(" ");
      sb.append(formatMessage(record));

      Throwable thrown = record.getThrown();
      if(thrown != null) {
        sb.append( System.getProperty("line.separator"));
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        record.getThrown().printStackTrace(pw);
        pw.close();
        sb.append(sw.toString());
      }
      return sb.toString();
    }
  }

  
  
}

