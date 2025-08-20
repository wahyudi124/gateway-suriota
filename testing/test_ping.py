import asyncio
from main import SuriotaGatewayClient

async def test_ping():
    client = SuriotaGatewayClient()
    try:
        connected = await client.connect()
        assert connected, "Failed to connect to BLE device"
        response = await client.ping()
        print(f"PING response: {response}")
        assert response.strip().upper().startswith("PONG"), f"Expected PONG, got: {response}"
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(test_ping())
