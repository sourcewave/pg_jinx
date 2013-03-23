package org.sourcewave.jinx.fdw;

public class Selection {
  public String column;
  public Object value;
  public enum Op {
    EQ, NE, LT, GT, LE, GE, LIKE;
    
    static public Op from(String s) {
      if (s.equals("=")) return EQ;
      else if (s.equals("<>")) return NE;
      else if (s.equals("<")) return LT;
      else if (s.equals(">")) return GT;
      else if (s.equals(">=")) return GE;
      else if (s.equals("<=")) return LE;
      else if (s.equals("~~")) return LIKE;
      else throw new java.lang.EnumConstantNotPresentException(Op.class , s );
    }
  }
  public Op op;
   
  public Selection(String c, String o, Object v) {
    column = c;
    op = Op.from(o);
    value = v;
  }
  
  @Override public String toString() {
    return "Selection("+column+" "+op+" "+value+")";
  }
}
