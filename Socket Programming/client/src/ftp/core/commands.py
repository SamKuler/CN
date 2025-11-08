"""
Command handlers for FTP commands
Extensible command registry pattern
:)
"""

from .parser import ResponseParser


class CommandHandler:
    """Base class for FTP command handlers"""
    
    def __init__(self, client):
        """
        Initialize command handler
        
        Args:
            client: FTPClient instance
        """
        self.client = client
    
    def execute(self, *args):
        """
        Execute the command
        
        Args:
            *args: Command arguments
            
        Returns:
            FTPResponse: Server response
        """
        raise NotImplementedError("Subclasses must implement execute()")
    
    def format_command(self, command, *args):
        """
        Format command string for sending
        
        Args:
            command: Command name
            *args: Command arguments
            
        Returns:
            str: Formatted command with CRLF
        """
        if args:
            return f"{command} {' '.join(str(a) for a in args)}\r\n"
        return f"{command}\r\n"


# FTP Command Handlers

class UserCommand(CommandHandler):
    """USER command handler"""
    
    def execute(self, username):
        """Send USER command"""
        cmd = self.format_command("USER", username)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class PassCommand(CommandHandler):
    """PASS command handler"""
    
    def execute(self, password):
        """Send PASS command"""
        cmd = self.format_command("PASS", password)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class PasvCommand(CommandHandler):
    """PASV command handler"""
    
    def execute(self):
        """Send PASV command and setup data connection"""
        cmd = self.format_command("PASV")
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        response = ResponseParser.parse(lines)
        
        if response.is_success:
            # Parse and setup passive data connection
            host, port = ResponseParser.parse_pasv_response(response)
            self.client.data_conn.setup_passive(host, port)
        
        return response


class PortCommand(CommandHandler):
    """PORT command handler"""
    
    def execute(self, host=None, port=0):
        """Send PORT command and setup data connection"""
        # Setup active data connection
        if host is None:
            # Use local address from control connection
            host = self.client.control_conn.sock.getsockname()[0]
        
        listen_host, listen_port = self.client.data_conn.setup_active(host, port)
        
        # Format and send PORT command
        port_arg = ResponseParser.format_port_command(listen_host, listen_port)
        cmd = self.format_command("PORT", port_arg)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class RetrCommand(CommandHandler):
    """RETR command handler for retrieving files"""
    
    def execute(self, filename):
        """Retrieve file from server"""
        # Send RETR command
        cmd = self.format_command("RETR", filename)
        self.client.control_conn.send(cmd)
        
        # Get preliminary response
        lines = self.client.control_conn.recv_multiline()
        response = ResponseParser.parse(lines)
        
        if response.is_preliminary or response.is_success:
            # Connect data channel and receive file
            self.client.data_conn.connect()
            data = self.client.data_conn.recv_all()
            self.client.data_conn.close()
            
            # Get completion response
            lines = self.client.control_conn.recv_multiline()
            final_response = ResponseParser.parse(lines)
            
            # Store received data in client
            self.client.last_transfer_data = data
            return final_response
        
        return response


class StorCommand(CommandHandler):
    """STOR command handler for storing files"""
    
    def execute(self, filename, data):
        """Store file on server"""
        # Send STOR command
        cmd = self.format_command("STOR", filename)
        self.client.control_conn.send(cmd)
        
        # Get preliminary response
        lines = self.client.control_conn.recv_multiline()
        response = ResponseParser.parse(lines)
        
        if response.is_preliminary or response.is_success:
            # Connect data channel and send file
            self.client.data_conn.connect()
            self.client.data_conn.send_data(data)
            self.client.data_conn.close()
            
            # Get completion response
            lines = self.client.control_conn.recv_multiline()
            return ResponseParser.parse(lines)
        
        return response


class ListCommand(CommandHandler):
    """LIST command handler"""
    
    def execute(self, path=''):
        """List directory contents"""
        # Send LIST command
        cmd = self.format_command("LIST", path) if path else self.format_command("LIST")
        self.client.control_conn.send(cmd)
        
        # Get preliminary response
        lines = self.client.control_conn.recv_multiline()
        response = ResponseParser.parse(lines)
        
        if response.is_preliminary or response.is_success:
            # Connect data channel and receive listing
            self.client.data_conn.connect()
            data = self.client.data_conn.recv_all()
            self.client.data_conn.close()
            
            # Get completion response
            lines = self.client.control_conn.recv_multiline()
            final_response = ResponseParser.parse(lines)
            
            # Store listing data
            self.client.last_transfer_data = data
            return final_response
        
        return response


class CwdCommand(CommandHandler):
    """CWD command handler"""
    
    def execute(self, directory):
        """Change working directory"""
        cmd = self.format_command("CWD", directory)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class PwdCommand(CommandHandler):
    """PWD command handler"""
    
    def execute(self):
        """Print working directory"""
        cmd = self.format_command("PWD")
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class MkdCommand(CommandHandler):
    """MKD command handler"""
    
    def execute(self, directory):
        """Make directory"""
        cmd = self.format_command("MKD", directory)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class RmdCommand(CommandHandler):
    """RMD command handler"""
    
    def execute(self, directory):
        """Remove directory"""
        cmd = self.format_command("RMD", directory)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class QuitCommand(CommandHandler):
    """QUIT command handler"""
    
    def execute(self):
        """Send QUIT command"""
        cmd = self.format_command("QUIT")
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class GenericCommand(CommandHandler):
    """Generic command handler for any FTP command"""
    
    def execute(self, command, *args):
        """
        Send any FTP command
        
        Args:
            command: Command name
            *args: Command arguments
        """
        cmd = self.format_command(command, *args)
        self.client.control_conn.send(cmd)
        lines = self.client.control_conn.recv_multiline()
        return ResponseParser.parse(lines)


class CommandRegistry:
    """Registry for FTP commands"""
    
    def __init__(self, client):
        """
        Initialize command registry
        
        Args:
            client: FTPClient instance
        """
        self.client = client
        self.commands = {}
        self._register_default_commands()
    
    def _register_default_commands(self):
        """Register default FTP commands"""
        self.register('USER', UserCommand)
        self.register('PASS', PassCommand)
        self.register('PASV', PasvCommand)
        self.register('PORT', PortCommand)
        self.register('RETR', RetrCommand)
        self.register('STOR', StorCommand)
        self.register('LIST', ListCommand)
        self.register('CWD', CwdCommand)
        self.register('PWD', PwdCommand)
        self.register('MKD', MkdCommand)
        self.register('RMD', RmdCommand)
        self.register('QUIT', QuitCommand)
    
    def register(self, command_name, handler_class):
        """
        Register a command handler
        
        Args:
            command_name: Command name (uppercase)
            handler_class: CommandHandler subclass
        """
        self.commands[command_name.upper()] = handler_class
    
    def get_handler(self, command_name):
        """
        Get handler for command
        
        Args:
            command_name: Command name
            
        Returns:
            CommandHandler: Handler instance
        """
        command_name = command_name.upper()
        handler_class = self.commands.get(command_name, GenericCommand)
        return handler_class(self.client)
    
    def execute(self, command_name, *args):
        """
        Execute a command
        
        Args:
            command_name: Command name
            *args: Command arguments
            
        Returns:
            FTPResponse: Server response
        """
        handler = self.get_handler(command_name)
        
        # For generic commands, pass command name as first argument
        if isinstance(handler, GenericCommand):
            return handler.execute(command_name, *args)
        
        return handler.execute(*args)