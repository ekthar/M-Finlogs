import { NextRequest } from "next/server";

/**
 * Server-Sent Events endpoint for real-time collaboration.
 * Clients subscribe to receive live updates (new transactions, edits, deletions).
 * Uses in-memory event bus (adequate for Vercel serverless with few users).
 */

interface SSEClient {
  id: string;
  controller: ReadableStreamDefaultController;
  companyId: string;
  username: string;
}

// Global event bus
const clients: SSEClient[] = [];

export function GET(request: NextRequest) {
  const { searchParams } = new URL(request.url);
  const companyId = searchParams.get("companyId") || "";
  const username = searchParams.get("username") || "anonymous";

  const stream = new ReadableStream({
    start(controller) {
      const clientId = `${username}_${Date.now()}`;
      const client: SSEClient = { id: clientId, controller, companyId, username };
      clients.push(client);

      // Send initial connection message
      const msg = `data: ${JSON.stringify({ type: "connected", clientId })}\n\n`;
      controller.enqueue(new TextEncoder().encode(msg));

      // Cleanup on close
      request.signal.addEventListener("abort", () => {
        const idx = clients.findIndex(c => c.id === clientId);
        if (idx !== -1) clients.splice(idx, 1);
      });
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      "Connection": "keep-alive",
    },
  });
}

/**
 * POST: Broadcast an event to all connected clients in the same company
 */
export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { companyId, type, payload, sender } = body;

    if (!companyId || !type) {
      return new Response(JSON.stringify({ error: "companyId and type required" }), { status: 400 });
    }

    const message = `data: ${JSON.stringify({ type, payload, sender, timestamp: Date.now() })}\n\n`;
    const encoded = new TextEncoder().encode(message);

    // Broadcast to all clients in the same company (except sender)
    const toRemove: number[] = [];
    clients.forEach((client, idx) => {
      if (client.companyId === companyId && client.username !== sender) {
        try {
          client.controller.enqueue(encoded);
        } catch {
          toRemove.push(idx);
        }
      }
    });

    // Remove dead connections
    toRemove.reverse().forEach(idx => clients.splice(idx, 1));

    return new Response(JSON.stringify({ status: "broadcast", recipients: clients.filter(c => c.companyId === companyId).length }));
  } catch {
    return new Response(JSON.stringify({ error: "Failed to broadcast" }), { status: 500 });
  }
}
