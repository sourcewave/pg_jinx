package org.sourcewave.jinx.example;

import java.awt.AlphaComposite;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.math.BigDecimal;
import java.net.URL;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;
import java.util.Random;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import javax.imageio.ImageIO;
import org.sourcewave.jinx.PostgresBridge;

public class Example {
  public static String randomString(int length) {
    char[] symbols = new char[36];
    char[] buf = new char[length];    
    for (int idx = 0; idx < 10; ++idx) symbols[idx] = (char) ('0' + idx);
    for (int idx = 10; idx < 36; ++idx) symbols[idx] = (char) ('a' + idx - 10);
    Random random = new Random();
    for (int idx = 0; idx < buf.length; ++idx) buf[idx] = symbols[random.nextInt(symbols.length)];
    return new String(buf);
  }
  
  public static BigDecimal doDecimal(BigDecimal d, BigDecimal n) {
    return d.divide(n);
  }
  
  public static Iterator<String[]> propertySet() {
    final Iterator<Entry<Object,Object>> p = System.getProperties().entrySet().iterator();
    return new Iterator<String[]>() {
      public boolean hasNext() { return p.hasNext(); }
      public String[] next() { Entry<Object,Object> e = (Entry<Object,Object>)p.next(); return new String[] {(String)e.getKey(), (String)e.getValue() }; }
      public void remove() { p.remove(); }
      
      
    };
  }
  
  
   public static Iterator<Object[]> unzip(final byte[] a) {
    // can I tell the difference between a tar and zip here?
    final ZipInputStream dis = new ZipInputStream(new ByteArrayInputStream(a));
    
    return new Iterator<Object[]>() {
      ZipEntry gne() {
        try { return dis.getNextEntry(); } catch(IOException iox) { return null; }
      }
      @Override public void remove() {
        throw new RuntimeException("Iterator.remove not implemented");
      }
      @Override public boolean hasNext() {
        try { return dis.available() > 0; }
        catch(IOException iox) { return false; }
      }
      
      @Override public Object[] next() {
        ZipEntry z = gne();
        if (z == null) return null;
        while(z.isDirectory()) { z = gne(); if (z == null) return null; } 
        String s = z.getName();
        Object[] result = new Object[2];
        result[0] = s;
        long n = z.getSize();
        byte[] buf;
        if ( n == -1) {
          ByteArrayOutputStream bos = new ByteArrayOutputStream();
          try { while(dis.available() == 1) { bos.write(dis.read()); } } catch(IOException iox) { return null; }
          buf = bos.toByteArray();
        } else { 
          buf = new byte[(int)n];
          try { dis.read(buf); } catch(IOException iox) { }
        }
        result[1] = buf;
        return result;
      }
    };
  }
   
  public static Iterator<Object[]> properties() {
    return new KvPair(System.getProperties());
  }
  private static class KvPair implements Iterator<Object[]> {
    private Properties props;
    private Iterator<Object> i;
    public KvPair(Properties p) {
      props = p;
      i = p.keySet().iterator();
    }
    
    @Override public boolean hasNext() { return i.hasNext(); }
    @Override public Object[] next() {
      if (hasNext()) {
        String s = i.next().toString();
        return new Object[]{ s, props.get(s).toString() } ;
      }
      return null;
    }
    
    @Override public void remove() {  }
  }
  
  public static int countRows(String table) throws SQLException {
    Object ox = PostgresBridge.execute(0, "select count(*) from "+table);
    return Integer.parseInt((String)((Object[])((Object[]) ox)[0])[0]);
  }
  
  public static String showTuple(Object[] t) {
    StringBuffer sb = new StringBuffer();
    for(Object a : t ) { sb.append(a.toString()); sb.append(" ; "); }
    return sb.toString();
  }
  public static Map<String,String> getOptions() throws SQLException {
    Object ox = PostgresBridge._execute(0, "show all", null);
    Map<String,String> guc = new HashMap<String,String>();
    for(Object oxa : (Object[])ox) {
      Object[] oxx = (Object[]) oxa;
      guc.put((String)oxx[0],(String)oxx[1]);
    }
    return guc;
  }
  
  public static Iterator<Object[]> rowCounts() throws SQLException {
    Connection conn = DriverManager.getConnection("jdbc:default:connection");
    PreparedStatement ps = conn.prepareStatement("select name,setting from pg_catalog.pg_settings");
    final ResultSet rs = ps.executeQuery();
    return new Iterator<Object[]>() {
      @Override public boolean hasNext() { try { return rs.next(); } catch(SQLException sqx) {return false; } }
      @Override public Object[] next() { try { return new Object[] { rs.getObject(1), rs.getObject(2), rs.getObject(3) }; } catch(SQLException sqx) { return new Object[]{null,null,null}; } }
      @Override public void remove() { return; }
    };
  }
  
  public static boolean[] allany(boolean[] arg) {
    boolean[] res = new boolean[] { true, false };
    for(boolean b : arg) { res[0] = res[0] && b; res[1] = res[1] || b; }
    return res;
  }
  
  public static String bytesToHex(byte[] bytes) {
    final char[] hexArray = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    char[] hexChars = new char[bytes.length * 2];
    int v;
    for ( int j = 0; j < bytes.length; j++ ) {
        v = bytes[j] & 0xFF;
        hexChars[j * 2] = hexArray[v >>> 4];
        hexChars[j * 2 + 1] = hexArray[v & 0x0F];
    }
    return new String(hexChars);
  }
  
  public static BufferedImage gravatar(String email ) {
    try { 
      URL url = new URL(getGravatarURL(email));
      return ImageIO.read( url.openStream() );
    } catch(NoSuchAlgorithmException | IOException x) {
      return null;
    }
    
  }
  
  public static String getGravatarURL(String email)
  throws NoSuchAlgorithmException, UnsupportedEncodingException {
    byte[] h  = MessageDigest.getInstance("MD5").digest( email.trim().toLowerCase().getBytes("UTF-8"));
    String hash = bytesToHex(h);
    return "http://www.gravatar.com/avatar/"+hash;
  }
  
  public static byte[] getGravatar(String email) throws IOException {
    BufferedImage im = gravatar(email);
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    ImageIO.write(im, "PNG", bos);
    byte[] bb = bos.toByteArray();
    return bb;
  }
  
  
  /*
  public static void registration(TriggerData td) throws SQLException {
    Connection conn = DriverManager.getConnection("jdbc:default:connection");
    PreparedStatement ps = conn.prepareStatement(
        "insert into public.user(email,confirmation_code,confirmation_requested_on) values (?, ?, ?)");
    if (td.isFiredByInsert()) {
      ps.setTimestamp(3, new Timestamp(System.currentTimeMillis()));
    } //else if (td.isFiredByUpdate()) {
      //ps.setString(1, "UPDATE");
    //} else if (td.isFiredByDelete()) {
//      ps.setString(1, "DELETE");
    //}
    
    ResultSet rs = td.getNew();
    rs.next();
    String cc = randomString(16);
    String to = rs.getString(1);
    ps.setString(1, to);
    ps.setString(2, cc);
    ps.execute();
    rs.close();
    ps.close();
    // TODO: this should localize the message
    ps = conn.prepareStatement("select * from gettext('registration_msg.html')");
    rs = ps.executeQuery();
    rs.next();
    String msg = rs.getString(1);
    rs.close();
    ps.close();
    
    msg = msg.replace("{{1}}","http://localhost:8080/register?confirmation="+cc );
    // TODO: this should read the subject from the database
    
    MailSender.sendTextMail(to, "Welcome to sharewave", msg);
    // conn.close();
  }
  */
    
  public static final int imageSize = 333;
  public static byte[] processImage(byte[] a) throws IOException {
    if (a == null) return null;
    BufferedImage im = ImageIO.read(new ByteArrayInputStream(a));
    if (im == null) return null;
    int y = im.getHeight();
    int x = im.getWidth();
    int ox = 0;
    int oy = 0;
    int nx = x;
    int ny = y;
    if (x > y) { 
      nx = imageSize; ny = (y * imageSize) / x;
      oy = (nx-ny)/2;
    } else {
      ny = imageSize; nx = (x * imageSize) / y;
      ox = (ny-nx)/2;
    }
    BufferedImage imx = new BufferedImage(imageSize, imageSize, BufferedImage.TYPE_INT_ARGB);
    
    Graphics2D g = imx.createGraphics();
    g.setComposite(AlphaComposite.Clear);
    g.fillRect(0, 0, imageSize, imageSize);
    g.setComposite(AlphaComposite.Src);

/*    g.clearRect(0, 0, imageSize, imageSize);
    g.setColor(new Color(0,0,0,1));
    g.fillRect(0, 0, imageSize, imageSize);
*/    
    
    g.drawImage(im, ox, oy, nx, ny, null); /*new ImageObserver() {
      public boolean imageUpdate(Image x, int a, int b, int c, int d, int e) {
        System.out.println(a + " / "+b+" / "+c+" / "+d + " / "+e);
        return false;
      }
    }); */ 
//    BufferedImage im0 = new BufferedImage(im,200, -200) ;Image.SCALE_AREA_AVERAGING);
    g.dispose();
    
    //WritableRaster raster = imx.getRaster();
    //DataBufferByte data   = (DataBufferByte) raster.getDataBuffer();
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    ImageIO.write(imx, "PNG", bos);
    byte[] bb = bos.toByteArray();
    return bb;
  }
  
}
