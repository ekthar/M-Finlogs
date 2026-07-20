# M-Finlogs Web

A full-stack Next.js web application for financial management and accounting. Cloud-hosted, accessible from anywhere.

## Tech Stack

- **Framework:** Next.js 14+ (App Router)
- **Language:** TypeScript
- **Styling:** Tailwind CSS 4 + Apple-style design system
- **Animation:** Framer Motion (spring-based, interruptible)
- **Database:** PostgreSQL (via Prisma ORM)
- **Auth:** NextAuth.js + JWT + Argon2 password hashing
- **Charts:** Recharts
- **Tables:** Custom data table components
- **PDF:** Server-side PDF generation
- **Email:** Nodemailer

## Getting Started

### Prerequisites

- Node.js 18+
- PostgreSQL database (Supabase, Neon, or local)

### Setup

```bash
# Install dependencies
npm install

# Copy environment file
cp .env.example .env
# Edit .env with your DATABASE_URL and secrets

# Generate Prisma client
npx prisma generate

# Run database migrations
npx prisma migrate dev

# Seed admin user (first run)
npx prisma db seed

# Start development server
npm run dev
```

Open [http://localhost:3000](http://localhost:3000)

### Deployment (Vercel)

```bash
# Push to GitHub and connect to Vercel
# Set environment variables in Vercel dashboard
# Deploy automatically on push
```

## Project Structure

```
src/
├── app/                    # Next.js App Router pages
│   ├── (auth)/            # Login, setup pages
│   ├── (dashboard)/       # Protected app pages
│   └── api/               # API routes
├── components/            # Reusable UI components
│   └── ui/               # Base design system primitives
├── lib/                   # Utilities, DB, auth config
└── middleware.ts          # Auth protection
prisma/
└── schema.prisma          # Database schema
```

## Design System

Built following Apple's interface design principles:
- Translucent materials (`backdrop-filter`) for depth hierarchy
- Spring-based animations (no fixed durations)
- Interruptible transitions
- Size-specific typography (tight tracking on headings)
- Instant feedback on interaction (press states)
- Reduced motion support
- Light/Dark theme

## Features

- Dashboard with KPI cards and charts
- Daily transaction entry with fast keyboard navigation
- Party Ledger, Day Book, Daily Summary reports
- Outstanding credit tracking with risk alerts
- Trial Balance & Profit/Loss
- Inventory management with 31-day grid
- PDF report generation with email delivery
- Multi-company & financial year support
- Role-based access (Admin / Accounts)
- Full audit logging
- Excel import/export
