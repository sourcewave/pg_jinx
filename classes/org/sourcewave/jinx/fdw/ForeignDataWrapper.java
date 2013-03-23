package org.sourcewave.jinx.fdw;

public abstract class ForeignDataWrapper {
  ColumnReference[] columns;
  ColumnReference[] extras;
  PgFunction [] selections;
  Column[] columnDefinitions;
  public int[] getForeignRelSize(ColumnReference[] cols, ColumnReference[] xtras, PgFunction[] ss) {
    columns = cols;
    extras = xtras;
    selections = ss;
    return new int[] {20, cols.length*15};
  }
  
  public void getForeignPaths() { }
  
  public void getForeignPlan() { }

  public void explainForeignScan() { }

  public void beginForeignScan() { }
  
  public void beginForeignScan(Column[] coldefs) {
    columnDefinitions = coldefs;
    beginForeignScan();
  }
  
  public Object[] iterateForeignScan() {
    return null;
  }
  
  public void reScanForeignScan() { }

  public void endForeignScan() { }

  public void cancelForeignScan() { }

  // for postgres pre 9.2
  @Deprecated public void planForeignScan() { }
}
