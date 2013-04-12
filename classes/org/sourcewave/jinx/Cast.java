package org.sourcewave.jinx;

import java.lang.annotation.Annotation;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.GenericArrayType;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Type;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

public class Cast {
  // The below copied from R0Kit
  // FIXME: use this to case from objects to Maps?
  public Map<String, Object> getPropertyMapFromClass(Class<?> cc) {
    Field[] fs = cc.getFields();
    Map<String, Object> res = new HashMap<String, Object>();
    for (Field f : fs) {
      res.put(f.getName(), f);
    }
    return res;
  }
  public static void setFields(Object o, Map<String, Object> m) {
    Class<?> c = o.getClass();
    for (Map.Entry<String, Object> e : m.entrySet()) {
      try {
        Field f = c.getField(e.getKey());
        f.set(o, cast(f.getType(), e.getValue()));
      } catch (NoSuchFieldException nsfe) {} catch (IllegalAccessException ignore) {}
    }
  }
  public static void setDeclaredFields(Object o, Map<String, Object> m) {
    Class<?> c = o.getClass();
    for (Map.Entry<String, Object> e : m.entrySet()) {
      try {
        Field f = c.getDeclaredField(e.getKey());
        f.set(o, cast(f.getType(), e.getValue()));
      } catch (NoSuchFieldException nsfe) {} catch (IllegalAccessException ignore) {}
    }
  }
  @SuppressWarnings("serial") public static class CastException extends RuntimeException {
    public CastException(String n) {
      super(n);
    }
  }
  private static Cast singleton = new Cast();
  public static <T> T cast(Class<T> t, Object z) {
    return singleton.doCast(t, z);
  }
  public static <T> T cast(Type t, Object z) {
    return singleton.doCast(t, z);
  }
  public static <T extends Collection<U>, U> T cast(Class<T> t, Class<U> h, Object z) {
    return singleton.subCast(t, h, z);
  }
  public static Object callStatic(String cl, String mth, Object[] args) throws InvocationTargetException, NoSuchMethodException,
      ClassNotFoundException {
    return singleton.callMethod(Class.forName(cl), null, null, mth, args);
  }
  public static Object callStatic(Class<?> cl, Class<? extends Annotation> a, String mth, Object[] args)
      throws InvocationTargetException, NoSuchMethodException {
    return singleton.callMethod(cl, null, a, mth, args);
  }
  public static Object callStatic(Class<?> cl, String mth, Object[] args) throws InvocationTargetException, NoSuchMethodException {
    return singleton.callMethod(cl, null, null, mth, args);
  }
  public static Object callMethod(Object o, Class<? extends Annotation> a, String mthnam, Object[] args)
      throws InvocationTargetException, NoSuchMethodException {
    return singleton.callMethod(o.getClass(), o, a, mthnam, args);
  }
  public static Object callMethod(Object o, String mthnam, Object[] args) throws InvocationTargetException, NoSuchMethodException {
    return singleton.callMethod(o.getClass(), o, null, mthnam, args);
  }
  @Retention(RetentionPolicy.RUNTIME) @Inherited public static @interface Castable {}
  @SuppressWarnings("unchecked") protected <T> T castInteger(Class<T> t, Integer z) {
    if (t == Long.class || t == long.class) return (T) new Long(z.longValue());
    if (t == Boolean.class || t == boolean.class) return (T) new Boolean(z == 1);
    if (t == Integer.class || t == int.class) return (T) z;
    if (t == Double.class || t == double.class) return (T) new Double(z.doubleValue());
    if (t == BigDecimal.class) return (T) new BigDecimal(z);
    if (t == BigInteger.class) return (T) BigInteger.valueOf(z);
    if (t.isEnum()) {
      T[] ec = t.getEnumConstants();
      String h = "$" + z;
      for (int i = 0; i < ec.length; i++)
        if (ec[i].toString().equals(h)) return ec[i];
      return ec[z];
    }
    if (t == String.class) return (T) Integer.toString(z);
    throw new CastException("Integer => " + t);
  }
  @SuppressWarnings("unchecked") protected <T> T castBoolean(Class<T> t, Boolean z) {
    if (t == Boolean.class || t == boolean.class) return (T) z;
    if (t == Integer.class || t == int.class) return (T) new Integer(z ? 1 : 0);
    if (t == Long.class || t == long.class) return (T) new Long(z ? 1 : 0);
    if (t == String.class) return (T) (z ? "true" : "false");
    throw new CastException("Boolean => " + t);
  }
  @SuppressWarnings("unchecked") protected <T> T castDouble(Class<T> t, Double z) {
    if (t == Long.class || t == long.class) return (T) new Long(z.longValue());
    if (t == Boolean.class || t == boolean.class) return (T) new Boolean(z == 1);
    if (t == Double.class || t == double.class) return (T) z;
    if (t == Integer.class || t == int.class) return (T) new Integer(z.intValue());
    throw new CastException("Double => " + t);
  }
  @SuppressWarnings("unchecked") protected <T> T castLong(Class<T> t, Long z) {
    if (t == Integer.class || t == int.class) return (T) new Integer(z.intValue());
    if (t == Boolean.class || t == boolean.class) return (T) new Boolean(z == 1);
    if (t == Long.class || t == long.class) return (T) z;
    if (t == Double.class || t == double.class) return (T) new Double(z.doubleValue());
    if (t == BigDecimal.class) return (T) new BigDecimal(z);
    if (t == BigInteger.class) return (T) BigInteger.valueOf(z);
    throw new CastException("Long => " + t);
  }
  @SuppressWarnings("unchecked") protected <T> T castEnum(Class<T> t, Enum<?> z) {
    if (t == String.class) return (T) z.toString();
    throw new CastException("String => " + t);
  }
  @SuppressWarnings("unchecked") protected <T> T castString(Class<T> t, String z) {
    if (t == String.class) return (T) z;
    // FIXME: do I care about encoding?
    if (t == byte[].class) return (T) z.getBytes();
    if (t == Character.class && z.length() == 1) return (T) new Character(z.charAt(0));
    if (t == Boolean.class || t == boolean.class) {
      if (z == null || z.length() < 1) return (T) new Boolean(false);
      Character k = z.charAt(0);
      return (T) new Boolean(k == '1' || k == 't' || k == 'T' || k == 'y' || k == 'Y');
    }
    if (t == Integer.class || t == int.class)
      return z.isEmpty() ? (T) new Integer(Integer.MIN_VALUE) : (T) new Integer(Integer.parseInt(z));
    if (t == Double.class || t == double.class) return (T) new Double(Double.parseDouble(z));
    if (t == Long.class || t == long.class) return (T) new Long(Long.parseLong(z));
    if (t.isEnum()) {
      if (z.isEmpty()) return null;
      for (Object nn : t.getEnumConstants())
        if (nn.toString().equalsIgnoreCase(z)) return (T) nn;
      for (int i = 0; i < z.length(); i++)
        if (!Character.isDigit(z.charAt(i))) throw new CastException("value " + z + " not member of enum " + t);
      return castInteger(t, Integer.parseInt(z));
    }
    // FIXME: this is a kludge
    if (t == String[].class) return (T) new String[] { z };
    if (t == int[].class) return (T) new int[] { Integer.parseInt(z) };
    throw new CastException("String => " + t);
  }
  @SuppressWarnings({ "unchecked", "rawtypes" }) public <T extends Collection<U>, U> T subCast(Class<T> r, Class<U> h, Object z) {
    T ll = null;
    if (z == null) return null;
    try {
      ll = r.getConstructor().newInstance();
    } catch (Exception e) {
      throw new CastException(" Array => " + r);
    }
    Type zox; // = z.getClass().getComponentType();
    zox = h;
    if (z instanceof Collection) {
      for (Object o : (Collection) z)
        ((Collection) ll).add(doCast(zox, o));
    } else if (z.getClass().isArray()) {
      int zl = Array.getLength(z);
      for (int i = 0; i < zl; i++) {
        Object next;
        try {
          next = doCast(zox, Array.get(z, i));
        } catch (Throwable t) {
          next = t;
        }
        ((Collection) ll).add(next);
      }
    } else if (((Class) h).isAssignableFrom(z.getClass())) {
      ((Collection) ll).add(z);
    } else ((Collection) ll).add(cast(h, z));
    // else throw new CastException( "unknown type assigning to Collection "+r+": "+z.getClass());
    return ll;
  }
  @SuppressWarnings({ "unchecked", "rawtypes" }) protected <T> T castArray(Class<T> t, Object z) {
    if (!z.getClass().isArray()) throw new IllegalArgumentException("castArray requires an Array argument");
    int zl = Array.getLength(z);
    Castable cc = t.getAnnotation(Castable.class);
    if (cc != null) {
      try {
        return t.getConstructor(z.getClass()).newInstance(z);
      } catch (Exception ite) {
        throw new CastException(z.getClass() + " => " + t);
      }
    }
    if (t.isArray()) {
      Class<?> ct = t.getComponentType();
      Object res = Array.newInstance(ct, zl);
      for (int i = 0; i < zl; i++) {
        Array.set(res, i, doCast(ct, Array.get(z, i)));
      }
      return (T) res;
    }
    if (Collection.class.isAssignableFrom(t)) {
      Collection ll = null;
      try {
        ll = (Collection) t.getConstructor().newInstance();
      } catch (Exception e) {
        throw new CastException(" Array => " + t);
      }
      Type zox = z.getClass().getComponentType();
      Type[] tx = t.getTypeParameters(); // getActualTypeArguments();
      zox = tx[0];
      for (int i = 0; i < zl; i++)
        ll.add(doCast(zox, Array.get(z, i)));
      return (T) ll;
    }
    // TODO: Think about how else to deal with this
    if (Array.getLength(z) == 1) return doCast(t, Array.get(z, 0)); // this is a massive kludge, but hey
    if (Array.getLength(z) == 0 && t == String.class) return (T) "";
    throw new CastException(z.getClass() + " => " + t);
  }
  @SuppressWarnings({ "rawtypes" }) protected Object castToArray(Type ct, Object z) {
    if (z.getClass().isArray()) {
      int zl = Array.getLength(z);
      Object res = Array.newInstance((Class<?>) ct, zl);
      for (int i = 0; i < zl; i++) {
        Array.set(res, i, doCast(ct, Array.get(z, i)));
      }
      return res;
    } else if (z instanceof Collection) {
      Object res = Array.newInstance((Class<?>) ct, ((Collection) z).size());
      int i = 0;
      for (Object zz : (Collection) z) {
        Array.set(res, i++, doCast(ct, zz));
      }
      return res;
    } else throw new CastException(z.getClass() + " => array of " + ct);
  }
  @SuppressWarnings("unchecked") protected <T> T castNull(Class<T> t) {
    if (!t.isPrimitive()) return null;
    if (t == int.class) return (T) new Integer(1 << 31);
    if (t == double.class) return (T) new Double(Double.NaN);
    if (t == boolean.class) return (T) new Boolean(false);
    throw new CastException("null => " + t);
  }
  protected <U, V> Map<U, V> castToMap(Class<? extends Map> r, Class<U> ck, Class<V> cv, Object z) {
    Map<U, V> res = null;
    if (r == Map.class) r = HashMap.class;
    try {
      res = (Map<U, V>) r.newInstance();
    } catch (IllegalAccessException x) {} catch (InstantiationException x) {
      throw new CastException(x.toString());
    }
    Map<U, V> mv = (Map<U, V>) z;
    for (Map.Entry<U, V> e : mv.entrySet()) {
      U k = cast(ck, e.getKey());
      V v = cast(cv, e.getValue());
      res.put(k, v);
    }
    return res;
  }
  @SuppressWarnings("unchecked") protected <T> T doCast(Type t, Object z) {
    if (t instanceof ParameterizedType) {
      Type r = ((ParameterizedType) t).getRawType();
      Type[] h = ((ParameterizedType) t).getActualTypeArguments();
      if (Map.class.isAssignableFrom((Class<?>) r)) { return (T) castToMap((Class<? extends Map>) r, (Class<?>) h[0],
          (Class<?>) h[1], z); }
      Class<? extends Object> hc;
      if (h[0] instanceof GenericArrayType) {
        GenericArrayType h0 = (GenericArrayType) h[0];
        Type ht = h0.getGenericComponentType();
        hc = (Class<?>) ht;
        hc = Array.newInstance(hc, 0).getClass();
      } else if (h[0] instanceof Class) hc = (Class<? extends Object>) h[0];
      // FIXME: I expect this to throw a ClassCastException
      else hc = (Class<? extends Object>) h[0];
      return (T) subCast((Class<? extends Collection>) r, hc, z);
    } else if (t instanceof GenericArrayType) {
      Type r = ((GenericArrayType) t).getGenericComponentType();
      return (T) castToArray(r, z);
    } else return doCast((Class<T>) t, z);
  }
  @SuppressWarnings("unchecked") protected <T> T doCast(Class<T> t, Object z) {
    if (z == null) return castNull(t);
    if (t == z.getClass()) return (T) z;
    if (t.isArray()) return (T) castToArray(t.getComponentType(), z);
    if (t.isAssignableFrom(z.getClass())) return t.cast(z);
    // FIXME: is the annotation really necessary?
    Castable cc = t.getAnnotation(Castable.class);
    if (cc != null) {
      try {
        Constructor<?> cos[] = t.getConstructors();
        for (Constructor<?> c : cos) {
          if (c.getParameterTypes().length != 1) continue;
          if (c.getParameterTypes()[0].isAssignableFrom(z.getClass())) { return (T) c.newInstance(z); }
        }
        // Constructor<T> co = t.getConstructor(z.getClass());
        // return co.newInstance(z);
        Method[] m = t.getMethods();
        for (int i = 0; i < m.length; i++) {
          if (m[i].getName().equals("constructor") && Modifier.isStatic(m[i].getModifiers())) {
            Class<?> ca[] = m[i].getParameterTypes();
            if (ca.length == 1 && ca[0].isAssignableFrom(z.getClass())) return (T) m[i].invoke(null, z);
          }
        }
      } catch (IllegalAccessException e) {
        throw new CastException(z.getClass() + " => " + t + " (" + e.getMessage() + ")");
      } catch (InvocationTargetException e) {
        throw new CastException(z.getClass() + " => " + t + " (" + e.getCause().getMessage() + ")");
      } catch (InstantiationException e) {
        throw new CastException(z.getClass() + " => " + t + " (" + e.getMessage() + ")");
      }
    }
    if (z instanceof String) return castString(t, (String) z);
    if (z instanceof Integer) return castInteger(t, (Integer) z);
    if (z instanceof Double) return castDouble(t, (Double) z);
    if (z instanceof Long) return castLong(t, (Long) z);
    if (z instanceof Boolean) return castBoolean(t, (Boolean) z);
    if (z.getClass().isEnum()) return castEnum(t, (Enum<?>) z);
    // FIXME: what about encoding?
    if (z instanceof byte[]) return castString(t, new String((byte[]) z));
    if (Map.class.isAssignableFrom(t)) {
      if (Map.class.isAssignableFrom(z.getClass())) {
        Map<Object, Object> res;
        try {
          res = (Map<Object, Object>) t.newInstance();
          for (Map.Entry<Object, Object> e : ((Map<Object, Object>) z).entrySet()) {
            res.put(e.getKey(), e.getValue());
          }
          return (T) res;
        } catch (IllegalAccessException iae) {} catch (InstantiationException ie) {}
      } else {
        Field[] fs = z.getClass().getFields();
        Map<String, Object> res;
        try {
          res = (Map<String, Object>) t.newInstance();
          for (Field f : fs) {
            res.put(f.getName(), f.get(z));
          }
          return (T) res;
        } catch (IllegalAccessException iae) {} catch (InstantiationException ie) {}
      }
    }
    if (z.getClass().isArray()) return castArray(t, z);
    throw new CastException(z.getClass() + " => " + t);
  }
  public static Class<?> boxClass(Class<?> z) {
    if (!z.isPrimitive()) return z;
    if (z == int.class) return Integer.class;
    if (z == double.class) return Double.class;
    if (z == boolean.class) return Boolean.class;
    if (z == long.class) return Long.class;
    if (z == short.class) return Short.class;
    if (z == byte.class) return Byte.class;
    throw new CastException("unimplemented primitive type: " + z);
  }
  protected Object callMethod(Class<?> clazz, Object o, Class<? extends Annotation> annot, String mthnam, Object[] args)
      throws InvocationTargetException, NoSuchMethodException {
    Method[] mth = clazz.getMethods();
    for (Method m : mth) {
      if (!m.getName().equals(mthnam)) continue;
      if (annot != null && null == m.getAnnotation(annot)) continue;
      Class<?>[] pt = m.getParameterTypes();
      if (pt.length != Array.getLength(args)) continue;
      int i = 0;
      Object[] methargs = new Object[pt.length];
      for (i = 0; i < pt.length; i++) {
        methargs[i] = doCast(pt[i], args[i]);
        if (pt[i].isPrimitive()) {
          Object c = null;
          try {
            c = methargs[i].getClass().getDeclaredField("TYPE").get(null);
          } catch (NoSuchFieldException nsfe) {} catch (IllegalAccessException iae) {}
          if (pt[i] != c) break;
        } else if (methargs[i] != null && !pt[i].isAssignableFrom(methargs[i].getClass())) break;
      }
      if (i < pt.length) continue;
      try {
        return m.invoke(o, methargs);
      } catch (IllegalAccessException iae) {
        continue;
      }
    }
    StringBuffer sb = new StringBuffer();
    for (Object a : args)
      sb.append("," + a.getClass());
    String s = clazz + "." + mthnam + "(" + (sb.length() > 0 ? sb.toString().substring(1) : "") + ")";
    throw new NoSuchMethodException(s);
  }
}
