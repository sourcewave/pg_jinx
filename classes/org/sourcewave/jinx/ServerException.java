
package org.sourcewave.jinx;

import java.sql.SQLException;

public class ServerException extends SQLException {
  private static final long serialVersionUID = 9082876915147363572L;
  
  public int elevel, saved_errno, sqlerrcode, lineno, cursorpos, internalpos;
  public boolean output_to_server, output_to_client, show_funcname;
  
  public String message, filename, funcname, detail, hint, context, internalquery, name;

  public ServerException(int[] edi, String[] eds) {
		super(eds[0]);
		elevel = edi[0];
		saved_errno = edi[1];
		sqlerrcode = edi[2];
		lineno = edi[3];
		cursorpos = edi[4];
		internalpos = edi[5];
		
		output_to_server = edi[6] != 0;
		output_to_client = edi[7] != 0;
		show_funcname = edi[8] != 0;
		
		message = eds[0];
		filename = eds[1];
		funcname = eds[2];
		detail = eds[3];
		hint = eds[4];
		context = eds[5];
		internalquery=eds[6];
		name = eds[7];
	}
}
