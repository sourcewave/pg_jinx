package org.sourcewave.jinx.fdw;

import org.sourcewave.jinx.DataType;


public class Column {
  public String name;
  public DataType type;
  public int more;
  public boolean notNull;
    
  public Column(String n, int t, int m, boolean nn) {
    name = n;
    type = DataType.fromOID(t);
    more = m;
    notNull = nn;
  }
  
  public @Override String toString() {
    return "Column("+name+" "+type+" "+more+ (notNull ? " NOT NULL" : "")+")";
  }
  
}
