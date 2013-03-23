package org.sourcewave.jinx.fdw;

public class ColumnReference {
  public String name;
  public int index;
  
  public ColumnReference(String s, int x) {
    name = s;
    index = x;
  }
  
  public @Override String toString() {
    return "ColumnReference("+name+","+index+")";
  }
}
