package org.sourcewave.jinx;

import java.io.IOException;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.GeneralSecurityException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.sql.SQLException;
import java.util.LinkedList;
import java.util.List;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import javax.security.cert.X509Certificate;

public class PostgresBridge {
  public static List<String> jarList() throws IOException {
    DirectoryStream<Path> st = Files.newDirectoryStream(Paths.get(System.getProperty("jinx.extDir")), "*.jar");
    List<String> ls = new LinkedList<String>();
    for(Path p : st) {
      ls.add(p.getFileName().toString());
    }
    return ls;
  }
  public static List<String> install_jar(byte[] b, String s) throws IOException {
    if (!s.endsWith(".jar")) s+= ".jar";
    Files.write( Paths.get(System.getProperty("jinx.extDir"),s), b);
    return jarList();
  }
  
  public static List<String> remove_jar(String s) throws IOException {
    if (!s.endsWith(".jar")) s+= ".jar";
    Path p = Paths.get(System.getProperty("jinx.extDir"),s);
    Files.delete(p);
    return jarList();
  }
  
  static {
    init();
  }
  
  public static void init() {

    //set up a TrustManager that trusts everything
    TrustManager[] trustAll = new TrustManager[] {
        
        new X509TrustManager() {
           public java.security.cert.X509Certificate[] getAcceptedIssuers() {
                   System.out.println("getAcceptedIssuers =============");
                   return null;
           }

           public void checkClientTrusted(X509Certificate[] certs, String authType) {
                   System.out.println("checkClientTrusted =============");
           }

           public void checkServerTrusted(X509Certificate[] certs, String authType) {
                   System.out.println("checkServerTrusted =============");
           }

          @Override public void checkClientTrusted(java.security.cert.X509Certificate[] arg0, String arg1)
              throws CertificateException {
            // TODO Auto-generated method stub
            System.out.println("checkClientTrusted ==========");
          }

          @Override public void checkServerTrusted(java.security.cert.X509Certificate[] arg0, String arg1)
              throws CertificateException {
            // TODO Auto-generated method stub
            
          }
        }
    };
    
 // Install the all-trusting trust manager
    try {
        SSLContext sc = SSLContext.getInstance("SSL"); 
        sc.init(null, trustAll, new java.security.SecureRandom()); 
        HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());
    } catch (GeneralSecurityException e) {
    } 
    
  }
    
  public static native void log(int logLevel, String str);

  public static native long _formTuple(long _this, Object[] values) throws SQLException;

  // from Relation
  public static native long _modifyTuple(long pointer, long original, int[] fieldNumbers, Object[] values) throws SQLException;
  
  public static native long _prepare(long threadId, String statement, int[] argTypes)
    throws SQLException;
  
  // portal
  public static native int _fetch(long pointer, long threadId, boolean forward, int count)
  throws SQLException;

  public static native int _move(long pointer, long threadId, boolean forward, int count) throws SQLException;


  public static native Object[] _execute(int num, String cmd,  Object[] parameters) throws ServerException;

  public static native int sp_start() throws ServerException;
  public static native void sp_commit() throws ServerException;
  public static native void sp_rollback() throws ServerException;
}

