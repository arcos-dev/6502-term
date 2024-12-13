# 6502-Term

## Description

6502-Term is a 6502 emulator written in C, designed to run and debug programs created for the 6502 microprocessor. It features accurate CPU emulation and tools to aid in step-by-step debugging, making it ideal for developers and enthusiasts working with retro computing.

## Features

- Accurate emulation of the 6502 CPU.
- Step-by-step debugging for detailed program analysis.
- Support for multiple memory configurations.
- Simple and intuitive terminal-based user interface.

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/arcosbr/6502-term.git
   ```

2. Navigate to the project directory:
   ```bash
   cd 6502-term
   ```

3. Compile the emulator:
   ```bash
   make
   ```

## Usage

Run the emulator with a ROM file:
```bash
./6502-term roms/hello.bin
```

In debugging mode, you can:
- Step through instructions.
- Inspect registers and memory.
- Set breakpoints for analysis.

## Contributing

Contributions are welcome! Here’s how you can help:

1. Fork the repository.
2. Create a new branch:
   ```bash
   git checkout -b feature-name
   ```
3. Commit your changes:
   ```bash
   git commit -m "Add a meaningful commit message"
   ```
4. Push to the branch:
   ```bash
   git push origin feature-name
   ```
5. Open a pull request.

Feel free to open an issue if you find bugs or have feature requests.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
