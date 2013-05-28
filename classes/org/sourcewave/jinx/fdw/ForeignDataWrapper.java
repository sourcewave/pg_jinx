package org.sourcewave.jinx.fdw;

public abstract class ForeignDataWrapper {
  protected ColumnReference[] columns;
  protected ColumnReference[] extras;
  protected PgFunction [] selections;
  protected Column[] columnDefinitions;
  
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

  
  public void addForeignUpdateTargets() {throw new RuntimeException("addForeignUpdateTargets not implemented"); }
  public void planForeignModify() {throw new RuntimeException("planForeignModify not implemented");}
  public void beginForeignModify() {throw new RuntimeException("beignForeignModify not implemented");}
  public void execForeignInsert() {throw new RuntimeException("execForeignInsert not implemented");}
  public void execForeignUpdate() {throw new RuntimeException("execForeignUpdate not implemented");}
  public void execForeignDelete() {throw new RuntimeException("execForeignDelete not implemented");}
  public void endForeignModify() {throw new RuntimeException("endForeignModify not implemented");}

  // for postgres pre 9.2
  @Deprecated public void planForeignScan() { }
}
