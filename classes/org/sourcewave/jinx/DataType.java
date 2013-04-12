package org.sourcewave.jinx;

import java.math.BigDecimal;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;
import java.sql.Types;
import java.util.HashMap;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

// from pg_type.h
public enum DataType {
  BOOLOID(16, Boolean.class), BYTEAOID(17, byte[].class), CHAROID(18, Byte.class),
  NAMEOID(19, String.class), INT8OID(20, Long.class), INT2OID(21, Short.class), INT2VECTOROID(22, short[].class),
  INT4OID(23, Integer.class),
  REGPROCOID(24), TEXTOID(25, String.class), OIDOID(26, Integer.class), TIDOID(27), XIDOID(28), CIDOID(29),
  OIDVECTOROID(30),
  POINTOID(600), LSEGOID(601), PATHOID(602), BOXOID(603), POLYGONOID(604),
  LINEOID(628),
  FLOAT4OID(700, Float.class), FLOAT8OID(701, Double.class), 
  ABSTIMEOID(702), RELTIMEOID(703), TINTERVALOID(704), UNKNOWNOID(705),
  CIRCLEOID(718), CASHOID(790), CSTRINGOID(2275, String.class), 
  INETOID(869), CIDROID(650), BPCHAROID(1042, String.class), VARCHAROID(1043, String.class),
  DATEOID(1082, Date.class), TIMEOID(1083, Time.class), TIMESTAMPOID(1114, Timestamp.class),
  TIMESTAMPTZOID(1184, Timestamp.class), INTERVALOID(1186), TIMETZOID(1266, Time.class),
  ZPBITOID(1560), VARBITOID(1562), NUMERICOID(1700, BigDecimal.class), VOIDOID(2278, Void.class),
  BOOLARRAYOID(1000, boolean[].class ), ANYOID(2276, Object.class), ANYELEMENTOID(2283, Object.class),
  RECORDOID(2249, Object[].class), RECORDARRAYOID(2287, Object[][].class) ;

  private int oid;
  private Class<?> clazz;
  private DataType(int x) { oid = x; clazz = null; }
  private DataType(int x, Class<?> cls) { oid = x; clazz = cls; }
  
  static public DataType fromOID(int oid) {
    for(DataType dd : DataType.values() ) {
      if (dd.oid == oid) return dd;
    }
    throw new java.lang.EnumConstantNotPresentException(DataType.class , Integer.toString(oid) );
  }

  private static Map<Class<?>, DataType> classMap;
  static {
    classMap = new HashMap<Class<?>, DataType>();
    classMap.put( Timestamp.class, TIMESTAMPOID);
    classMap.put( Time.class, TIMEOID);
    classMap.put( String.class, VARCHAROID);
    classMap.put( Short.class, INT2OID);
    classMap.put( Integer.class, INT4OID);
    classMap.put( Float.class, FLOAT4OID);
    classMap.put( Long.class, INT8OID);
    classMap.put( Double.class, FLOAT8OID);
    classMap.put( Date.class, DATEOID);
    classMap.put( byte[].class, BYTEAOID);
    classMap.put( Byte.class, CHAROID);
    classMap.put( Boolean.class, BOOLOID);
    classMap.put( BigDecimal.class, NUMERICOID);
    classMap.put( boolean[].class,  BOOLARRAYOID);
    classMap.put( Object[].class, RECORDOID);
    classMap.put( Object[][].class, RECORDARRAYOID);
  }
  
  public static DataType forJavaClass(Class<?> c) {
    return classMap.get(c);
  }
  
  public int getOid() { return oid; }
  
  public Class<?> getJavaClass() throws SQLException {
    if (clazz == null) throw new SQLException("OID not mapped to java class: "+oid);
    return clazz;
  }

  public static DataType forSqlType(int sqlType) throws SQLException {
    switch(sqlType) {
      case Types.BIT: return ZPBITOID; 
      case Types.TINYINT: return CHAROID;
      case Types.SMALLINT: return INT2OID; 
      case Types.INTEGER:  return INT4OID;
      case Types.BIGINT:   return INT8OID;
      case Types.FLOAT:    
      case Types.REAL: return FLOAT4OID;
      case Types.DOUBLE: return FLOAT8OID;
      case Types.NUMERIC:
      case Types.DECIMAL: return NUMERICOID;
      case Types.DATE: return DATEOID;
      case Types.TIME: return TIMEOID;
      case Types.TIMESTAMP: return TIMESTAMPOID;
      case Types.BOOLEAN: return BOOLOID;
      case Types.BINARY:
      case Types.VARBINARY:
      case Types.LONGVARBINARY:
      case Types.BLOB: return BYTEAOID; // BYTEAOID
      case Types.CHAR:
      case Types.VARCHAR:
      case Types.LONGVARCHAR:
      case Types.CLOB:
      case Types.DATALINK: return TEXTOID;

      default:
        throw new SQLException("SQL Type not yet mapped: "+sqlType);  /* Not yet mapped */
    }
  }

  public Object cast(Object x) {
    try { return Cast.cast(getJavaClass(), x); }
    catch(SQLException sqx) {
      Logger.getAnonymousLogger().log(Level.SEVERE, sqx.toString());
      return null; }
  }
  
}
