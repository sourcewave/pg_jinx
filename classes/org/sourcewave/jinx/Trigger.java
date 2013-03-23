package org.sourcewave.jinx;

public abstract class Trigger {
  protected Object[] OLD;
  protected Object[] NEW;
  protected boolean[] flags;
  
  public abstract void invoke() throws Throwable;
}
