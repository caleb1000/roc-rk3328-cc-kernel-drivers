
import matplotlib.pyplot as plt
import numpy as np
import socket
import struct
import threading
from pynput.mouse import Listener, Button, Controller

max_radius = 10000
#TO:DO this should be behind a mutex to prevent a write after read error
cur_radius = 5000
min_radius = 1000

def plot_lidar_data(ax, data_set, user_input):
    # Remove tuples with distance 0.00
    valid_data = [(distance, angle) for distance, angle in data_set if distance != 0.00]

    # Convert angles to radians and mirror them around the 180/0 degree mark
    angles_rad = np.radians([angle if angle <= np.pi else 2 * np.pi - angle for _, angle in valid_data])

    # Get distances as radii
    radii = [distance for distance, _ in valid_data]

    # Clear the previous plot
    ax.clear()
    # Plot the lidar data on a unit circle with colors based on distance values
    if user_input == '2':
        ax.axis('off')
        ax.scatter(angles_rad, radii, c=radii, cmap='hsv', s=1, vmin=0, vmax=5000)
    else:
        ax.set_title('Lidar Data on Unit Circle (Distances in mm)', color = 'white')
        ax.scatter(angles_rad, radii, c=radii, cmap='hsv', marker='o', label='Lidar Data', s=1, vmin=0, vmax=5000)
        ax.legend(facecolor='black', edgecolor='black', fontsize='small', labelcolor='red')
    
    # Add a grid
    ax.grid(True)
    ax.set_rmax(cur_radius)
    ax.tick_params(axis='x', colors='white')
    ax.tick_params(axis='y', colors='white')

# Create a mouse controller instance
mouse = Controller()

def on_scroll(x, y, dx, dy):
    global cur_radius
    if dy < 0:  # Scrolling down
        cur_radius += 1000
        if cur_radius > max_radius:
            cur_radius = max_radius
        #print('Scrolled up: cur_radius =', cur_radius)
    elif dy > 0:  # Scrolling up
        cur_radius -= 1000
        if cur_radius < min_radius:
            cur_radius = min_radius
        #print('Scrolled down: cur_radius =', cur_radius)

def start_listener():
    with Listener(on_scroll=on_scroll) as listener:
        listener.join()


def main():

    # Start the listener function in a separate thread
    listener_thread = threading.Thread(target=start_listener)
    listener_thread.start()

    UDP_PORT = 6969
    BUFFER_SIZE = 320  # Allocate room for 40 floating point values

    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Bind the socket to the address and port
    sock.bind(("0.0.0.0", UDP_PORT))

    # Prompt the user for input
    user_input = input("Enter '1' for Unit Circle or '2' for Minimal: ")

    # Check the user input and perform corresponding actions
    if user_input == '1':
        print("Unit Circle selected.")
        # Create a figure and axis for the plot
        plt.figure(facecolor='grey')
    elif user_input == '2':
        print("Minimal selected.")
        # Create a figure and axis for the plot
        plt.rcParams['toolbar'] = 'None'
        plt.figure(facecolor='black')
        
    else:
        print("Error, invalid input. Exitting...")
        return -1
    
    print("Waiting for data...")

    ax = plt.subplot(111, projection='polar')
    ax.set_facecolor('black')
    # Set a timeout of .01 seconds
    sock.settimeout(0.01)
    recv_buffer = []
    prev_tuples = []
    # Continuously update and display the lidar plot
    while True:
        try:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            recv_buffer.extend(struct.unpack('80f', data))
            if len(recv_buffer) < 1000:
                continue


            # Initialize empty arrays to store even and odd-indexed values
            even_values = []
            odd_values = []

            # Extract even and odd-indexed values
            for i in range(len(recv_buffer)):
                if i % 2 == 0:
                    even_values.append(recv_buffer[i])
                else:
                    odd_values.append(recv_buffer[i])

            # Convert the lists of values to NumPy arrays
            even_values_array = np.array(even_values, dtype=float)
            odd_values_array = np.array(odd_values, dtype=float)

            # Create a list of tuples containing even and odd-indexed values, removing tuples where the first value is zero
            distance_angle_tuple = [(distance, angle) for distance, angle in zip(even_values_array, odd_values_array) if distance != 0]
            temp_tuples = distance_angle_tuple.copy()
            distance_angle_tuple.extend(prev_tuples)
            # Update and display lidar plot
            plot_lidar_data(ax, distance_angle_tuple, user_input)
            
            plt.draw()
            recv_buffer.clear()

            #prev_tuples.clear()
            prev_tuples = temp_tuples
            plt.pause(0.001)

        except socket.timeout:
            continue
            #print("Timeout occurred. No data received within the specified timeout.")


if __name__ == "__main__":
    main()
