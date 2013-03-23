package org.sourcewave.jinx;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.net.URI;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import javax.tools.Diagnostic;
import javax.tools.DiagnosticCollector;
import javax.tools.FileObject;
import javax.tools.ForwardingJavaFileManager;
import javax.tools.JavaCompiler;
import javax.tools.JavaCompiler.CompilationTask;
import javax.tools.JavaFileManager;
import javax.tools.JavaFileObject;
import javax.tools.JavaFileObject.Kind;
import javax.tools.SimpleJavaFileObject;
import javax.tools.StandardJavaFileManager;
import javax.tools.ToolProvider;

public class Compiler {
  
  // For the compiler: a String that looks like a File
  public class StringJavaFileObject extends SimpleJavaFileObject {
    public String name, source;
    public StringJavaFileObject(String nam, String src) {
        super(URI.create("string:///"+nam.replace('.','/')+Kind.SOURCE.extension), Kind.SOURCE);
        this.name = nam;
        this.source = src;
    }
    @Override public CharSequence getCharContent(boolean ignoreEncodingErrors) { return this.source; }
  }

  // For the compiler: a ByteArray that looks like a File
  public class ByteArrayJavaFileObject extends SimpleJavaFileObject {
    private final ByteArrayOutputStream bos = new ByteArrayOutputStream();
    public String className;
    public ByteArrayJavaFileObject(String name, Kind knd) {
        super(URI.create("string:///" + name.replace('.', '/') + knd.extension), knd);
    }
    public byte[] getBytes() { return bos.toByteArray(); }      
    @Override public OutputStream openOutputStream() { return bos; }
  }

  // For the compiler: a ClassLoader that loads a ByteArray
  public class ByteArrayClassLoader extends ClassLoader {
    private Map<String, ByteArrayJavaFileObject> cache = new HashMap<String, ByteArrayJavaFileObject>();
    
    public ByteArrayClassLoader() { super(ByteArrayClassLoader.class.getClassLoader()); }

    public void put(String name, ByteArrayJavaFileObject obj) { cache.put(name, obj); }

    @Override protected Class<?> findClass(String name) throws ClassNotFoundException {
        ByteArrayJavaFileObject co = cache.get(name);
        if (co == null) throw new ClassNotFoundException(name);
        try { byte[] ba = co.getBytes(); return defineClass(co.className, ba, 0, ba.length); }
        catch (Exception ex) { throw new ClassNotFoundException(name, ex); }
    } 
  }

  // For the compiler: a ClassFileManager that uses in-memory inputs
  public class DynamicClassFileManager <FileManager> extends ForwardingJavaFileManager<JavaFileManager> {
    private ByteArrayClassLoader loader = null;
    DynamicClassFileManager(StandardJavaFileManager mgr) {
        super(mgr);
        try {loader = new  ByteArrayClassLoader(); } catch (Exception ex) { }
    }

    @Override public JavaFileObject getJavaFileForOutput(Location loc, String name, Kind kind, FileObject sibling)
            throws IOException {
        ByteArrayJavaFileObject bajfo = new ByteArrayJavaFileObject(name,kind);
        loader.put(name, bajfo);
        bajfo.className = name;
        if (sibling instanceof StringJavaFileObject) loader.put(((StringJavaFileObject)sibling).name, bajfo); // store it here as well
        return bajfo;
    }

    @Override public ClassLoader getClassLoader(Location loc) { return loader; }
  }

  // for the Compiler: compiling a String (src) as the Class (nam) 
  private Class<?> compileIt(String nam, String src) throws ClassNotFoundException {
    JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
    DiagnosticCollector<JavaFileObject> collector = new DiagnosticCollector<JavaFileObject>();
    JavaFileManager manager = new DynamicClassFileManager<JavaFileManager>(compiler.getStandardFileManager(null, null,null));
    JavaFileObject strFile = new StringJavaFileObject(nam, src);
    Iterable<? extends JavaFileObject> units = Arrays.asList(strFile);
    CompilationTask task = compiler.getTask(null, manager, collector, null, null, units);
    boolean status = task.call();
    if (status) return manager.getClassLoader(null).loadClass(nam);
    StringBuffer sb = new StringBuffer();
    for(Diagnostic<?> d: collector.getDiagnostics()) { 
      sb.append("on line "+d.getLineNumber()+", column "+d.getColumnNumber()+": "+d.getMessage(null)); sb.append("\n"); }
    throw new ClassNotFoundException(sb.toString());
  }

  // for the Compiler: compile a method into a class
  public static Method compileExpression(String src, String[] paramNames, String[] paramTypes) throws ClassNotFoundException {
    String nam = "Class"+Math.abs(new Random().nextInt());
    StringBuilder src2 = new StringBuilder();
    src2.append("package org.sourcewave.jinx.dynamic; public class ");
    src2.append(nam);
    src2.append(" { public static Object doit(");
    boolean first = true;
    for(int i=0;i<paramNames.length;i++) {
      if (first) first = false; else src2.append(", ");
      src2.append(paramTypes[i]+" "+paramNames[i]);
    }
    src2.append(") {");
    src2.append(src);
    src2.append("} }");    
    Class<?> clazz = new Compiler().compileIt(nam, src2.toString());
    return clazz.getMethods()[0];
  }

  
  
}
