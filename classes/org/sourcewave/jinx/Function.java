package org.sourcewave.jinx;

import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

public class Function {
  int oid;
  Class <?> clazz;
  Method method;
  Class<?> returnType;
  
  static HashMap<Integer,Function> functionMap = new HashMap<Integer,Function>();
  
  public Function(int o) {
    oid = o;
    functionMap.put(oid, this);
  }
  
  public static Function fromOID(int o) {
    Function f = functionMap.get(o);
    if (f == null) f = new Function(o);
    return f;
  }
  
  public Object[] triggerInvoke(String source, String name, boolean[] bits, Object[] oldt, Object[] newt) 
  throws Throwable {
    int n = source.lastIndexOf('.');
    String cn = source.substring(0,n);
    String mn = source.substring(n+1);
    clazz = Class.forName(cn);
    Trigger instance = (Trigger)clazz.newInstance();
    instance.OLD = oldt;
    instance.NEW = newt;
    instance.flags = bits;
    instance.invoke();
    return instance.OLD;
  }
  
  public Object invoke( String name, String[] paramNames, int[] t, int r, Object[] args ) throws Throwable {
      // throws ClassNotFoundException, NoSuchMethodException {
    
    // Backend.log( ELogHandler.LOG_WARNING, name);
    
    boolean namonly = false;
    namonly = name.matches("([\\p{L}\\p{Pc}][\\p{L}\\p{Pc}\\p{N}]+\\.)+[\\p{L}\\p{Pc}][\\p{L}\\p{Pc}\\p{N}]+");
    if (!namonly) {
      String[] tt = new String[t.length];
      for(int i=0;i<tt.length;i++) tt[i]=DataType.fromOID(t[i]).getJavaClass().getSimpleName();
      method = Compiler.compileExpression(name, paramNames, tt);
      clazz = method.getDeclaringClass();
    } else {
    int n = name.lastIndexOf('.');
    String cn = name.substring(0,n);
    String mn = name.substring(n+1);
    clazz = Class.forName(cn);
    Class<?>[] pt = new Class<?>[t.length];
    for(int i=0;i<t.length;i++) {
      pt[i]=DataType.fromOID(t[i]).getJavaClass();
    }
    // Backend.log( ELogHandler.LOG_WARNING, args[0].toString() );
    
    Method[] ms = clazz.getMethods();
    List<Method> candidates = new LinkedList<Method>();
    outerloop:
      for(Method mm : ms) {
        if (!mm.getName().equals(mn)) continue;
        Class<?>[] mpt = mm.getParameterTypes();
        if (pt.length != mpt.length) continue;
        Class<?>[] pt2 = new Class<?>[pt.length];
        for(int i=0;i<mpt.length;i++) {
          pt2[i]=pt[i];
          if (mpt[i].isPrimitive()) {
            Object c = null;
            try {
              c = pt[i].getDeclaredField("TYPE").get(null);
              pt2[i] = (Class<?>)c;
            } catch (NoSuchFieldException nsfe) {} catch (IllegalAccessException iae) {}
          }
        }
        innerloop:
          for(int i=0;i<pt2.length;i++) {
            if (pt2[i] == mpt[i]) continue;
            if (mpt[i].isAssignableFrom(pt2[i])) continue;
            continue outerloop;
          }
        candidates.add(mm);
      }
    if (candidates.size() == 1) method = candidates.get(0);
    else if (candidates.size() == 0) throw new NoSuchMethodException(mn);
    else throw new RuntimeException("ambiguous method call: "+mn);
    }
    // method = clazz.getMethod(mn, pt);
    try {
      Object x = method.invoke( clazz.newInstance(), args );
      // Backend.log( ELogHandler.LOG_WARNING, x == null ? "null" : x.toString() );
      if (x == null) return x;
      if (x instanceof Map) { return new MapIterator( (Map)x); }
      if (x instanceof List) { return ((List)x).iterator(); }
      if (x instanceof Set) { return ((Set)x).iterator(); }
      if (x instanceof Collection) { return ((Collection)x).iterator(); }
      if (x instanceof byte[]) return x;
      if (x.getClass().isArray()) { return new ArrayIterator(x); }
      return x;
    } catch(InvocationTargetException ite) {
      throw ite.getTargetException(); 
    }
  }
  static class MapIterator implements Iterator {
    Iterator<Entry> map;
    MapIterator(Map x) { map = x.entrySet().iterator(); }
    @Override public boolean hasNext() { return map.hasNext(); }
    @Override public Object next() { Entry e = map.next(); return new Object[] { e.getKey(), e.getValue() };  }
    @Override public void remove() { }
  }
  
  static class ArrayIterator implements Iterator { 
    Object array;
    int pos;
    ArrayIterator(Object x) { array = x; pos = 0; }
    @Override public boolean hasNext() { return pos < Array.getLength(array); }
    @Override public Object next() { return Array.get(array, pos++);  }
    @Override public void remove() { }
    
  }
}
