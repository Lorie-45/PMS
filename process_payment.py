import serial
import pandas as pd
from datetime import datetime
import time

SERIAL_PORT = 'COM10'  # Update as needed
BAUD_RATE = 9600
CSV_FILE = 'plates_log.csv'
PARKING_RATE_PER_HOUR = 200  # RWF


def load_csv():
    try:
        return pd.read_csv(CSV_FILE)
    except FileNotFoundError:
        # Create a new DataFrame if file doesn't exist
        return pd.DataFrame(columns=[
            'Plate Number',
            'Timestamp',
            'Exit Time',
            'Due Amount',
            'Payment Status'
        ])


def save_csv(df):
    df.to_csv(CSV_FILE, index=False)


def calculate_due(entry_time_str, exit_time):
    entry_time = datetime.strptime(entry_time_str, '%Y-%m-%d %H:%M:%S')
    duration = exit_time - entry_time
    hours = duration.total_seconds() / 3600
    hours_rounded = int(hours) if hours == int(hours) else int(hours) + 1
    due = hours_rounded * PARKING_RATE_PER_HOUR
    return due, hours_rounded


def main():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)  # Wait for Arduino reset

    print("Parking Management System - Ready")
    print("Waiting for plate number from Arduino...")

    while True:
        line = ser.readline().decode('utf-8').strip()

        if line.startswith("PLATE:"):
            plate = line.split("PLATE:")[1].strip()
            print(f"\nVehicle Plate: {plate}")

            # Get current balance from Arduino
            balance_line = ""
            start_time = time.time()
            while time.time() - start_time < 5:
                if ser.in_waiting:
                    balance_line = ser.readline().decode('utf-8').strip()
                    if balance_line.startswith("CURRENT_BALANCE:"):
                        break

            if not balance_line.startswith("CURRENT_BALANCE:"):
                print("Error: Could not read card balance")
                ser.write(b"STATUS:NO_BALANCE\n")
                continue

            current_balance = int(balance_line.split("CURRENT_BALANCE:")[1].strip())
            print(f"Current Card Balance: {current_balance} RWF")

            df = load_csv()
            row_idx = df.index[df['Plate Number'] == plate].tolist()

            if not row_idx:
                print(f"Error: Plate {plate} not in system")
                ser.write(b"STATUS:NOT_FOUND\n")
                continue

            idx = row_idx[0]
            status = df.at[idx, 'Payment Status']
            entry_time_str = df.at[idx, 'Timestamp']

            if status == 1:
                print("Status: Already paid")
                ser.write(b"STATUS:PAID\n")
                continue

            # Calculate parking fee
            exit_time = datetime.now()
            due_amount, hours_parked = calculate_due(entry_time_str, exit_time)

            print(f"Parking Duration: {hours_parked} hours")
            print(f"Parking Fee: {due_amount} RWF")

            # Update CSV with exit time and due amount
            df.at[idx, 'Exit Time'] = exit_time.strftime('%Y-%m-%d %H:%M:%S')
            df.at[idx, 'Due Amount'] = due_amount
            save_csv(df)

            # Send due amount to Arduino
            ser.write(f"DUE:{due_amount}\n".encode())
            print("\nProcessing payment...")

            # Wait for payment result
            confirmation = ""
            start_time = time.time()
            while time.time() - start_time < 10:
                if ser.in_waiting:
                    confirmation = ser.readline().decode('utf-8').strip()
                    if confirmation.startswith(("PAYMENT_SUCCESS:", "INSUFFICIENT_FUNDS:")):
                        break

            if confirmation.startswith("PAYMENT_SUCCESS:"):
                # Display payment confirmation and new balance
                parts = confirmation.split("PAYMENT_SUCCESS:")[1].split(",")
                paid_amount = int(parts[0].split("=")[1].strip())
                new_balance = int(parts[1].split("=")[1].strip())

                print("\n=== Payment Successful ===")
                print(f"Amount Paid: {paid_amount} RWF")
                print(f"Remaining Balance: {new_balance} RWF")
                print("=========================")

                # Update payment status only (no balance storage)
                df.at[idx, 'Payment Status'] = 1
                save_csv(df)

                ser.write(b"STATUS:PAID\n")

            elif confirmation.startswith("INSUFFICIENT_FUNDS:"):
                # Display insufficient funds message
                parts = confirmation.split("INSUFFICIENT_FUNDS:")[1].split(",")
                current_bal = int(parts[0].split("=")[1].strip())
                required = int(parts[1].split("=")[1].strip())

                print("\n!!! Payment Failed !!!")
                print(f"Current Balance: {current_bal} RWF")
                print(f"Required Amount: {required} RWF")
                print("Please top up your card")
                print("========================")
                ser.write(b"STATUS:INSUFFICIENT\n")

            else:
                print("Error: Payment processing failed")
                ser.write(b"STATUS:FAILED\n")

        time.sleep(0.1)


if __name__ == "__main__":
    main()