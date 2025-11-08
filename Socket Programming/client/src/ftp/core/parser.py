"""
Response parser for FTP protocol
Handles parsing of server responses
"""


class FTPResponse:
    """Represents an FTP server response"""
    
    def __init__(self, code, message, lines=None):
        """
        Initialize FTP response
        
        Args:
            code: Response code (e.g., 220, 230)
            message: Response message
            lines: All response lines for multiline responses
        """
        self.code = code
        self.message = message
        self.lines = lines or [f"{code} {message}"]
        
    @property
    def is_success(self):
        """Check if response indicates success (2xx or 3xx)"""
        return self.code // 100 in (2, 3)
    
    @property
    def is_error(self):
        """Check if response indicates error (4xx or 5xx)"""
        return self.code // 100 in (4, 5)
    
    @property
    def is_preliminary(self):
        """Check if response is preliminary (1xx)"""
        return self.code // 100 == 1
    
    def __str__(self):
        return f"{self.code} {self.message}"
    
    def __repr__(self):
        return f"FTPResponse({self.code}, {self.message!r})"


class ResponseParser:
    """
    Parses FTP server responses
    or formats command arguments
    """
    
    @staticmethod
    def parse(lines):
        """
        Parse response lines into FTPResponse object
        
        Args:
            lines: List of response lines or single line string
            
        Returns:
            FTPResponse: Parsed response object
        """
        if isinstance(lines, str):
            lines = [lines]
        
        if not lines:
            raise ValueError("Empty response")
        
        first_line = lines[0]
        
        # Extract code and message from first line
        try:
            code = int(first_line[:3])
            message = first_line[4:] if len(first_line) > 4 else ""
        except (ValueError, IndexError):
            raise ValueError(f"Invalid response format: {first_line}")
        
        # For multiline responses, combine all messages
        if len(lines) > 1:
            all_messages = [message]
            for line in lines[1:]:
                if len(line) > 4:
                    all_messages.append(line[4:])
                else:
                    all_messages.append(line)
            message = '\n'.join(all_messages)
        
        return FTPResponse(code, message, lines)
    
    @staticmethod
    def parse_pasv_response(response):
        """
        Parse PASV response to extract host and port
        
        Args:
            response: FTPResponse from PASV command
            
        Returns:
            tuple: (host, port)
        """
        # PASV response format: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
        message = response.message
        
        # Find the parentheses
        start = message.find('(')
        end = message.find(')')
        
        if start == -1 or end == -1:
            raise ValueError(f"Invalid PASV response: {message}")
        
        # Extract numbers
        numbers = message[start+1:end].split(',')
        if len(numbers) != 6:
            raise ValueError(f"Invalid PASV response format: {message}")
        
        try:
            h1, h2, h3, h4, p1, p2 = map(int, numbers)
            host = f"{h1}.{h2}.{h3}.{h4}"
            port = p1 * 256 + p2
            return host, port
        except ValueError:
            raise ValueError(f"Invalid PASV response numbers: {message}")
    
    @staticmethod
    def format_port_command(host, port):
        """
        Format PORT command argument
        
        Args:
            host: IP address
            port: Port number
            
        Returns:
            str: Formatted PORT command argument
        """
        # Convert host to h1,h2,h3,h4
        host_parts = host.split('.')
        if len(host_parts) != 4:
            raise ValueError(f"Invalid host address: {host}")
        
        # Convert port to p1,p2
        p1 = port // 256
        p2 = port % 256
        
        return ','.join(host_parts + [str(p1), str(p2)])