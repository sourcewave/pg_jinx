package org.sourcewave.jinx;

import java.sql.SQLException;

public class PostgresBridge {
  
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


  public static native Object[] _execute(int num, String cmd,  Object[] parameters) throws ServerException;

  public static native int sp_start() throws ServerException;
  public static native void sp_commit() throws ServerException;
  public static native void sp_rollback() throws ServerException;
}

